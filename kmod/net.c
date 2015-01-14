#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "net"

#define LISTEN_RESTART_TIMEOUT_MS 2000

#define KLOG_SOCK(s, msg)						\
	KLOG(KL_INF, "%s socket %p self %08x:%u peer %08x:%u",		\
		msg, s, ksock_self_addr(s), ksock_self_port(s),		\
			ksock_peer_addr(s), ksock_peer_port(s));

static struct kmem_cache *server_cachep;
static struct kmem_cache *con_cachep;

static DEFINE_MUTEX(srv_list_lock);
static LIST_HEAD(srv_list);

static void ds_con_wait(struct ds_con *con)
{
	kthread_stop(con->thread);
}

static void ds_con_free(struct ds_con *con)
{
	kmem_cache_free(con_cachep, con);
}

static void ds_con_release(struct ds_con *con)
{
	KLOG(KL_DBG, "releasing sock %p", con->sock);
	ksock_release(con->sock);
	put_task_struct(con->thread);
	ds_con_free(con);
}

static void ds_con_fail(struct ds_con *con, int err)
{
	if (!con->err) {
		KLOG(KL_INF, "con %p failed err %x", con, err);
		con->err = err;
	}
}

static int ds_con_recv(struct ds_con *con, void *buffer, u32 nob)
{
	u32 read;
	int err;

	if (con->err)
		return con->err;

	err = ksock_read(con->sock, buffer, nob, &read);
	if (err) {
		ds_con_fail(con, err);
		return err;
	}

	if (nob != read) {
		KLOG(KL_ERR, "nob %u read %u", nob, read);
		err = -EIO;
		ds_con_fail(con, err);
		return err;
	}

	return 0;
}

static int ds_con_send(struct ds_con *con, void *buffer, u32 nob)
{
	u32 wrote;
	int err;

	if (con->err)
		return con->err;

	err = ksock_write(con->sock, buffer, nob, &wrote);
	if (err) {
		ds_con_fail(con, err);
		return err;
	}

	if (nob != wrote) {
		KLOG(KL_ERR, "nob %u wrote %u", nob, wrote);
		err = -EIO;
		ds_con_fail(con, err);
		return err;
	}

	return 0;
}

static int ds_con_reply(struct ds_con *con,
		struct ds_net_pkt *reply, int err)
{
	reply->err = err;
	net_pkt_sign(reply);
	err = ds_con_send(con, reply, sizeof(*reply));
	if (err) {
		KLOG(KL_ERR, "reply send err %d", err);
	}

	return err;
}

static int ds_con_recv_pages(struct ds_con *con,
	struct ds_net_pkt *pkt, struct ds_pages *ppages)
{
	struct sha256_sum dsum;
	struct ds_pages pages;
	int err;
	u32 read, llen;
	void *buf;
	int i;

	if (pkt->dsize == 0 || pkt->dsize > DS_NET_PKT_MAX_DSIZE) {
		KLOG(KL_ERR, "dsize %u invalid", pkt->dsize);
		return -EINVAL;
	}

	err = ds_pages_create(pkt->dsize, &pages);
	if (err) {
		KLOG(KL_ERR, "no memory");
		return err;
	}	

	read = pkt->dsize;
	i = 0;
	llen = 0;
	while (read > 0) {
		KLOG(KL_DBG, "read %u nr_pages %u i %u, llen %u dsize %u plen %u",
			read, pages.nr_pages, i, llen, pkt->dsize, pages.len);
		DS_BUG_ON(i >= pages.nr_pages);
		buf = kmap(pages.pages[i]);
		llen = (read > PAGE_SIZE) ? PAGE_SIZE : read;
		err = ds_con_recv(con, buf, llen);
		kunmap(pages.pages[i]);
		if (err) {
			KLOG(KL_ERR, "read err %d", err);
			ds_con_fail(con, err);
			goto free_pages;
		}
		i++;
		read-= llen;
	}

	ds_pages_dsum(&pages, &dsum);
	err = net_pkt_check_dsum(pkt, &dsum);
	if (err) {
		KLOG(KL_ERR, "invalid dsum");
		goto free_pages;
	}

	memcpy(ppages, &pages, sizeof(pages));
	return 0;

free_pages:
	ds_pages_release(&pages);
	return err;
}

static int ds_con_send_pages(struct ds_con *con,
	struct ds_pages *pages)
{
	u32 i, len, ilen;
	void *ibuf;
	int err;

	len = pages->len;
	i = 0;
	while (len > 0) {
		BUG_ON(i >= pages->nr_pages);
		ilen = (len > PAGE_SIZE) ? PAGE_SIZE : len;
		ibuf = kmap(pages->pages[i]);
		err = ds_con_send(con, ibuf, ilen);
		kunmap(pages->pages[i]);	
		if (err) {
			ds_con_fail(con, err);
			goto out;
		}	
		len-= ilen;
		i++;
	}
	err = 0;
out:
	return err;
}

static int ds_con_get_obj(struct ds_con *con, struct ds_net_pkt *pkt,
	struct ds_net_pkt *reply)
{
	int err;
	struct ds_pages pages;
	struct sha256_sum dsum;

	if (pkt->dsize == 0 || pkt->dsize > DS_NET_PKT_MAX_DSIZE) {
		KLOG(KL_ERR, "dsize %u invalid", pkt->dsize);
		return -EINVAL;
	}

	err = ds_pages_create(pkt->dsize, &pages);
	if (err) {
		ds_con_reply(con, reply, err);
		goto out;
	}

	err = ds_sb_list_get_obj(&pkt->u.get_obj.obj_id,
		pkt->u.get_obj.off,
		0,
		pkt->dsize,
		pages.pages,
		pages.nr_pages);
	if (err) {
		ds_con_reply(con, reply, err);
		goto free_pages;
	}
	ds_pages_dsum(&pages, &dsum);
	memcpy(&reply->dsum, &dsum, sizeof(dsum));
	reply->dsize = pkt->dsize;

	err = ds_con_reply(con, reply, 0);
	if (err) {
		goto free_pages;	
	}

	err = ds_con_send_pages(con, &pages);

free_pages:
	ds_pages_release(&pages);
out:
	return err;
}

static int ds_con_put_obj(struct ds_con *con, struct ds_net_pkt *pkt,
	struct ds_net_pkt *reply)
{
	int err;
	struct ds_pages pages;

	err = ds_con_recv_pages(con, pkt, &pages);
	if (err) {
		if (!con->err)
			ds_con_reply(con, reply, err);
		goto out;
	}
	
	err = ds_sb_list_put_obj(&pkt->u.put_obj.obj_id,
		pkt->u.put_obj.off,
		0,
		pkt->dsize,
		pages.pages,
		pages.nr_pages);

	err = ds_con_reply(con, reply, err);

	ds_pages_release(&pages);
out:
	return err;
}

static int ds_con_create_obj(struct ds_con *con, struct ds_net_pkt *pkt,
	struct ds_net_pkt *reply)
{
	int err;

	err = ds_sb_list_create_obj(&reply->u.create_obj.obj_id);
	return ds_con_reply(con, reply, err);
}

static int ds_con_delete_obj(struct ds_con *con, struct ds_net_pkt *pkt,
	struct ds_net_pkt *reply)
{
	int err;

	err = ds_sb_list_delete_obj(&pkt->u.delete_obj.obj_id);
	return ds_con_reply(con, reply, err);
}

static int ds_con_echo(struct ds_con *con, struct ds_net_pkt *pkt,
	struct ds_net_pkt *reply)
{
	return ds_con_reply(con, reply, 0);
}

static int ds_con_process_pkt(struct ds_con *con, struct ds_net_pkt *pkt)
{
	struct ds_net_pkt *reply;
	int err;

	reply = net_pkt_alloc();
	if (!reply) {
		err = -ENOMEM;
		KLOG(KL_ERR, "no memory");
		ds_con_fail(con, err);
		return err;
	}

	KLOG(KL_INF, "pkt %d", pkt->type);

	switch (pkt->type) {
		case DS_NET_PKT_ECHO:
			err = ds_con_echo(con, pkt, reply);
			break;
		case DS_NET_PKT_PUT_OBJ:
			err = ds_con_put_obj(con, pkt, reply);
			break;
		case DS_NET_PKT_GET_OBJ:
			err = ds_con_get_obj(con, pkt, reply);
			break;
		case DS_NET_PKT_DELETE_OBJ:
			err = ds_con_delete_obj(con, pkt, reply);
			break;
		case DS_NET_PKT_CREATE_OBJ:
			err = ds_con_create_obj(con, pkt, reply);
			break;
		default:
			err = ds_con_reply(con, reply, -EINVAL);
			break;
	}

	crt_free(reply);
	return err;
}

static int ds_con_thread_routine(void *data)
{
	struct ds_con *con = (struct ds_con *)data;
	struct ds_server *server = con->server;
	int err;

	BUG_ON(con->thread != current);

	KLOG_SOCK(con->sock, "con starting");

	while (!kthread_should_stop() && !con->err) {
		struct ds_net_pkt *pkt = net_pkt_alloc();
		if (!pkt) {
			ds_con_fail(con, -ENOMEM);
			KLOG(KL_ERR, "no memory");
			break;
		}
		
		err = ds_con_recv(con, pkt, sizeof(*pkt));
		if (err) {
			KLOG(KL_ERR, "socket recv err %d", err);
			crt_free(pkt);
			break;
		}

		if ((err = net_pkt_check(pkt))) {
			KLOG(KL_ERR, "pkt check err %d", err);
			crt_free(pkt);
			ds_con_fail(con, -EINVAL);
			break;
		}

		ds_con_process_pkt(con, pkt);
		crt_free(pkt);
	}

	KLOG_SOCK(con->sock, "con stopping");
	KLOG(KL_INF, "stopping con %p err %d", con, con->err);

	if (!server->stopping) {
		mutex_lock(&server->con_list_lock);
		if (!list_empty(&con->con_list))
			list_del_init(&con->con_list);	
		else
			con = NULL;
		mutex_unlock(&server->con_list_lock);

		if (con)
			ds_con_release(con);
	}

	return 0;
}

static struct ds_con *ds_con_start(struct ds_server *server,
	struct socket *sock)
{
	struct ds_con *con;
	int err = -EINVAL;

	con = kmem_cache_alloc(con_cachep, GFP_NOIO);
	if (!con) {
		KLOG(KL_ERR, "cant alloc ds_con");
		return NULL;
	}
	memset(con, 0, sizeof(*con));

	con->server = server;
	con->thread = NULL;
	con->sock = sock;
	con->thread = kthread_create(ds_con_thread_routine, con, "ds_con");
	if (IS_ERR(con->thread)) {
		err = PTR_ERR(con->thread);
		con->thread = NULL;
		KLOG(KL_ERR, "kthread_create err=%d", err);
		goto out;
	}

	get_task_struct(con->thread);	
	mutex_lock(&server->con_list_lock);
	list_add_tail(&con->con_list, &server->con_list);
	mutex_unlock(&server->con_list_lock);

	wake_up_process(con->thread);

	return con;	
out:
	ds_con_free(con);
	return NULL;
}

static void ds_server_free(struct ds_server *server)
{
	kmem_cache_free(server_cachep, server);
}

static int ds_server_thread_routine(void *data)
{
	struct ds_server *server = (struct ds_server *)data;
	struct socket *lsock = NULL;
	struct socket *con_sock = NULL;
	struct ds_con *con = NULL;
	int err = 0;
	u32 listen_attempts = 3;

	if (server->thread != current)
		BUG_ON(1);

	while (!kthread_should_stop()) {
		if (!server->sock) {
			KLOG(KL_INF, "start listen on ip %x port %d",
				server->ip, server->port);
			err = ksock_listen(&lsock, (server->ip) ? server->ip :
				INADDR_ANY, server->port, 5);
			if (err == -EADDRINUSE && listen_attempts) {
				KLOG(KL_WRN, "csock_listen err=%d", err);
				msleep_interruptible(LISTEN_RESTART_TIMEOUT_MS);
				if (listen_attempts > 0)
					listen_attempts--;
				continue;
			} else if (!err) {
				KLOG(KL_DBG, "listen done ip %x port %d",
						server->ip, server->port);
				mutex_lock(&server->lock);
				server->sock = lsock;
				KLOG_SOCK(server->sock, "listened");
				mutex_unlock(&server->lock);
			} else {
				KLOG(KL_ERR, "csock_listen err=%d", err);	
			}
			server->err = err;
			complete(&server->comp);
			if (err)
				break;
		}

		if (server->sock && !server->stopping) {
			KLOG(KL_DBG, "accepting");
			err = ksock_accept(&con_sock, server->sock);
			if (err) {
				if (err == -EAGAIN) {
					KLOG(KL_WRN, "accept err=%d", err);
				} else {
					KLOG(KL_ERR, "accept err=%d", err);
				}
				continue;
			}
			KLOG_SOCK(con_sock, "accepted");
			if (!ds_con_start(server, con_sock)) {
				KLOG(KL_ERR, "ds_con_start failed");
				ksock_release(con_sock);
				continue;
			}
		}
	}

	err = 0;
	mutex_lock(&server->lock);
	lsock = server->sock;
	KLOG(KL_DBG, "releasing listen socket=%p", lsock);
	server->sock = NULL;
	mutex_unlock(&server->lock);

	if (lsock)
		ksock_release(lsock);
	
	KLOG(KL_DBG, "releasing cons");

	for (;;) {
		con = NULL;
		mutex_lock(&server->con_list_lock);
		if (!list_empty(&server->con_list)) {
			con = list_first_entry(&server->con_list, struct ds_con,
					con_list);
			list_del_init(&con->con_list);		
		}
		mutex_unlock(&server->con_list_lock);
		if (!con)
			break;

		ds_con_wait(con);
		ds_con_free(con);
	}

	KLOG(KL_DBG, "released cons");
	return 0;
}

static void ds_server_do_stop(struct ds_server *server)
{
	if (server->stopping) {
		KLOG(KL_ERR, "server %p %u-%d already stopping",
			server, server->ip, server->port);
		return;
	}

	server->stopping = 1;
	if (server->sock) {
		ksock_abort_accept(server->sock);
	}

	kthread_stop(server->thread);
	put_task_struct(server->thread);
	KLOG(KL_INF, "stopped server on ip %x port %d",
		server->ip, server->port);
}

static struct ds_server *ds_server_create_start(u32 ip, int port)
{
	char thread_name[10];
	int err;
	struct ds_server *server;

	server = kmem_cache_alloc(server_cachep, GFP_NOIO);
	if (!server) {
		KLOG(KL_ERR, "no memory");
		return NULL;
	}

	memset(server, 0, sizeof(*server));
	INIT_LIST_HEAD(&server->con_list);
	INIT_LIST_HEAD(&server->srv_list);
	mutex_init(&server->lock);
	mutex_init(&server->con_list_lock);
	server->port = port;
	server->ip = ip;
	init_completion(&server->comp);

	snprintf(thread_name, sizeof(thread_name), "%s-%d", "ds_srv", port);
	server->thread = kthread_create(ds_server_thread_routine, server, thread_name);
	if (IS_ERR(server->thread)) {
		err = PTR_ERR(server->thread);
		server->thread = NULL;
		KLOG(KL_ERR, "kthread_create err=%d", err);
		ds_server_free(server);
		return NULL;
	}
	get_task_struct(server->thread);
	wake_up_process(server->thread);
	wait_for_completion(&server->comp);
	
	return server;
}

int ds_server_start(u32 ip, int port)
{
	int err;
	struct ds_server *server;

	mutex_lock(&srv_list_lock);
	list_for_each_entry(server, &srv_list, srv_list) {
		if (server->port == port && server->ip == ip) {
			KLOG(KL_INF, "server for ip %x port %d already exists",
				ip, port);
			err = -EEXIST;
			mutex_unlock(&srv_list_lock);
			return err;
		}
	}
	server = ds_server_create_start(ip, port);
	if (server && !server->err) {
		KLOG(KL_INF, "started server on ip %x port %d",
			ip, port);
		list_add_tail(&server->srv_list, &srv_list);
		err = 0;
	} else {
		err = (!server) ? -ENOMEM : server->err;
		if (server) {
			ds_server_do_stop(server);
			ds_server_free(server);
		}
	}
	mutex_unlock(&srv_list_lock);

	return err;
}

int ds_server_stop(u32 ip, int port)
{
	int err = -EINVAL;
	struct ds_server *server;

	mutex_lock(&srv_list_lock);
	list_for_each_entry(server, &srv_list, srv_list) {
		if (server->port == port && server->ip == ip) {
			ds_server_do_stop(server);
			list_del(&server->srv_list);
			ds_server_free(server);
			err = 0;
			break;
		}
	}
	mutex_unlock(&srv_list_lock);
	return err;
}

static void ds_server_stop_all(void)
{
	struct ds_server *server;
	struct ds_server *tmp;
	mutex_lock(&srv_list_lock);
	list_for_each_entry_safe(server, tmp, &srv_list, srv_list) {
		ds_server_do_stop(server);
		list_del(&server->srv_list);
		ds_server_free(server);
	}
	mutex_unlock(&srv_list_lock);
}

int ds_server_init(void)
{
	int err;

	server_cachep = kmem_cache_create("server_cache",
			sizeof(struct ds_server), 0,
			SLAB_MEM_SPREAD, NULL);
	if (!server_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto out;
	}

	con_cachep = kmem_cache_create("con_cache",
			sizeof(struct ds_con), 0,
			SLAB_MEM_SPREAD, NULL);
	if (!con_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto del_server_cache;
	}

	return 0;

del_server_cache:
	kmem_cache_destroy(server_cachep);
out:
	return err;
}

void ds_server_finit(void)
{
	ds_server_stop_all();
	kmem_cache_destroy(server_cachep);
	kmem_cache_destroy(con_cachep);
}
