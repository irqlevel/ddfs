#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/highmem.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cdrom.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

#include <ds.h>
#include <ds_cmd.h>
#include <klog.h>
#include <ksocket.h>
#include <ds_priv.h>


#define __SUBCOMPONENT__ "ds-dev"
#define __LOGNAME__ "ds.log"

static DEFINE_MUTEX(dev_list_lock);
static LIST_HEAD(dev_list);


static void ds_dev_free(struct ds_dev *dev)
{
	if (dev->dev_name)
		kfree(dev->dev_name);
	kfree(dev);
}

static int ds_dev_insert(struct ds_dev *cand)
{
	struct ds_dev *dev;
	int err;

	mutex_lock(&dev_list_lock);
	list_for_each_entry(dev, &dev_list, dev_list) {
		if (0 == strncmp(dev->dev_name, cand->dev_name,
			strlen(cand->dev_name)+1)) {
			err = -EEXIST;
			break;
		}
	}
	list_add_tail(&cand->dev_list, &dev_list);
	err = 0;
	mutex_unlock(&dev_list_lock);
	return err;
}

static void ds_dev_release(struct ds_dev *dev)
{
	klog(KL_DBG, "releasing dev=%p bdev=%p", dev, dev->bdev);

	if (dev->bdev)
		blkdev_put(dev->bdev, FMODE_READ|FMODE_WRITE|FMODE_EXCL);
}

static void ds_dev_unlink(struct ds_dev *dev)
{
	mutex_lock(&dev_list_lock);
	list_del(&dev->dev_list);
	mutex_unlock(&dev_list_lock);
}

static struct ds_dev *ds_dev_lookup_unlink(char *dev_name)
{
	struct ds_dev *dev;

	mutex_lock(&dev_list_lock);
	list_for_each_entry(dev, &dev_list, dev_list) {
		if (0 == strncmp(dev->dev_name, dev_name,
			strlen(dev_name)+1)) {
			list_del(&dev->dev_list);
			mutex_unlock(&dev_list_lock);
			return dev;
		}
	}
	mutex_unlock(&dev_list_lock);
	return NULL;
}

static struct ds_dev *ds_dev_create(char *dev_name)
{
	struct ds_dev *dev;
	int len;
	int err;

	len = strlen(dev_name);
	if (len == 0) {
		klog(KL_ERR, "len=%d", len);
		return NULL;
	}

	dev = kmalloc(sizeof(struct ds_dev), GFP_KERNEL);
	if (!dev) {
		klog(KL_ERR, "dev alloc failed");
		return NULL;
	}

	memset(dev, 0, sizeof(*dev));
	dev->dev_name = kmalloc(len + 1, GFP_KERNEL);
	if (!dev->dev_name) {
		klog(KL_ERR, "dev_name alloc failed");
		ds_dev_free(dev);
		return NULL;
	}
	spin_lock_init(&dev->io_lock);
	INIT_LIST_HEAD(&dev->io_list);

	memcpy(dev->dev_name, dev_name, len + 1);
	dev->bdev = blkdev_get_by_path(dev->dev_name,
		FMODE_READ|FMODE_WRITE|FMODE_EXCL, dev);
	if ((err = IS_ERR(dev->bdev))) {
		dev->bdev = NULL;
		klog(KL_ERR, "bkdev_get_by_path failed err %d", err);
		ds_dev_free(dev);
		
		return NULL;
	}

	return dev;
}

static int ds_dev_thread_routine(void *data)
{
	struct ds_dev *dev = (struct ds_dev *)data;
	int err = 0;

	klog(KL_DBG, "dev %p thread starting", dev);

	if (dev->thread != current)
		BUG_ON(1);

	err = ds_dev_io_touch0_page(dev);
	if (err) {
		klog(KL_ERR, "ds_dev_touch0_page dev %p err %d",
			dev, err);
	}

	klog(KL_DBG, "going to run main loop dev=%p", dev);
	while (!kthread_should_stop()) {
		msleep_interruptible(100);
		if (dev->stopping)
			break;
	}

	klog(KL_DBG, "dev %p exiting", dev);
	return err;
}

static int ds_dev_start(struct ds_dev *dev)
{
	int err;
	dev->thread = kthread_create(ds_dev_thread_routine, dev, "ds_dev_th");
	if (IS_ERR(dev->thread)) {
		err = PTR_ERR(dev->thread);
		dev->thread = NULL;
		klog(KL_ERR, "kthread_create err=%d", err);
		return err;
	}
	get_task_struct(dev->thread);
	wake_up_process(dev->thread);
	err = 0;
	return err;
}

static void ds_dev_stop(struct ds_dev *dev)
{
	dev->stopping = 1;
	if (dev->thread) {
		kthread_stop(dev->thread);
		put_task_struct(dev->thread);
	}
	while (!list_empty(&dev->io_list)) {
		msleep_interruptible(50);	
	}
}

int ds_dev_add(char *dev_name)
{
	int err;
	struct ds_dev *dev;

	klog(KL_DBG, "inserting dev %s", dev_name);
	dev = ds_dev_create(dev_name);
	if (!dev) {
		return -ENOMEM;
	}

	err = ds_dev_insert(dev);
	if (err) {
		klog(KL_ERR, "ds_dev_insert err %d", err);
		ds_dev_release(dev);
		ds_dev_free(dev);
		return err;
	}

	err = ds_dev_start(dev);
	if (err) {
		klog(KL_ERR, "ds_dev_insert err %d", err);
		ds_dev_unlink(dev);		
		ds_dev_release(dev);
		ds_dev_free(dev);
		return err;
	}

	return err;
}

int ds_dev_remove(char *dev_name)
{
	int err;
	struct ds_dev *dev;

	klog(KL_DBG, "removing dev %s", dev_name);
	dev = ds_dev_lookup_unlink(dev_name);
	if (dev) {
		ds_dev_stop(dev);
		ds_dev_release(dev);
		ds_dev_free(dev);
		err = 0;
	} else {
		klog(KL_ERR, "dev with name %s not found", dev_name);
		err = -ENOENT;
	}

	return err;
}

void ds_dev_release_all(void)
{
	struct ds_dev *dev;
	struct ds_dev *tmp;
	mutex_lock(&dev_list_lock);
	list_for_each_entry_safe(dev, tmp, &dev_list, dev_list) {
		ds_dev_stop(dev);
		ds_dev_release(dev);
		list_del(&dev->dev_list);
		ds_dev_free(dev);
	}
	mutex_unlock(&dev_list_lock);
}

