int dev_random,dev_urandom;

int ds_random_buf_read(void *buf, size_t len, int urandom)
{
		int err;
		
		err = read((urandom) ? dev_urandom : dev_random, buf, len);
		
		return err;
}

int ds_random_init(void)
{
		dev_random = open("/dev/random", O_RDONLY, 0);
		if (dev_random<0)
				return -ENOMEM;
				
		dev_urandom = open("/dev/urandom", O_RDONLY, 0);
		if (dev_urandom<0) 
				return -ENOMEM;
		return 0;
}

void ds_random_release(void)
{
		close(dev_random);
		close(dev_urandom);
}
