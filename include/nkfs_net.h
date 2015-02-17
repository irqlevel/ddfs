#pragma once

#include <include/nkfs_types.h>
#include <include/nkfs_obj_info.h>
#include <include/nkfs_const.h>
#include <crt/include/csum.h>

enum {
	NKFS_NET_PKT_ECHO = 1,
	NKFS_NET_PKT_GET_OBJ,
	NKFS_NET_PKT_PUT_OBJ,
	NKFS_NET_PKT_DELETE_OBJ,
	NKFS_NET_PKT_QUERY_OBJ,
	NKFS_NET_PKT_CREATE_OBJ,
	NKFS_NET_PKT_NEIGH_HANDSHAKE
};

#define NKFS_NET_PKT_SIGN1	((u32)0xBEDABEDA)
#define NKFS_NET_PKT_SIGN2	((u32)0xCBADCBAD)

#pragma pack(push, 1)
struct nkfs_net_pkt {
	u32			sign1;
	u32			type;
	u32			dsize; /* data(after header) size */
	int			err;
	union {
		struct {
			struct nkfs_obj_id	obj_id;
			u64			off;
		} get_obj;
		struct {
			struct nkfs_obj_id	obj_id;
			u64			off;
		} put_obj;
		struct {
			struct nkfs_obj_id	obj_id;
		} delete_obj;
		struct {
			struct nkfs_obj_id	obj_id;
		} create_obj;
		struct {
			struct nkfs_obj_id	obj_id;
			struct nkfs_obj_info	obj_info;
		} query_obj;
		struct {
			struct nkfs_obj_id	net_id;
			struct nkfs_obj_id	host_id;
			u32			d_ip;
			int			d_port;
			u32			s_ip;
			int			s_port;
			struct nkfs_obj_id	reply_host_id;
		} neigh_handshake;
	} u;
	struct csum		dsum;
	struct csum		sum;	
	u32			sign2;
};
#pragma pack(pop)