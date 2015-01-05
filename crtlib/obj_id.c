#include <crtlib/include/crtlib.h>

asmlinkage char *ds_obj_id_to_str(struct ds_obj_id *id)
{
	return bytes_hex((char *)id, sizeof(*id));
}

asmlinkage int ds_obj_id_gen(struct ds_obj_id *id)
{
	return crt_random_buf(id, sizeof(*id));
}

asmlinkage int ds_obj_id_cmp(struct ds_obj_id *id1, struct ds_obj_id *id2)
{
	return crt_memcmp(id1, id2, sizeof(*id2));
}

asmlinkage struct ds_obj_id *ds_obj_id_create(void)
{
	struct ds_obj_id *id;
	int err;

	id = crt_malloc(sizeof(struct ds_obj_id));
	if (!id) {
		return NULL;
	}

	err = ds_obj_id_gen(id);
	if (err) {
		crt_free(id);
		return NULL;
	}

	return id;
}
