#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "balloc"

int ds_balloc_bm_clear(struct ds_sb *sb)
{
	struct dio_cluster *clu;
	u64 i;
	int err;

	for (i = sb->bm_block; i < sb->bm_block + sb->bm_blocks; i++) {
		clu = dio_clu_get(sb->ddev, i);
		if (!clu) {
			KLOG(KL_ERR, "cant read block %llu", i);
			err = -EIO;
			goto out;
		}

		err = dio_clu_zero(clu);
		if (err) {
			KLOG(KL_ERR, "cant zero clu %llu err %d", i, err);
			dio_clu_put(clu);
			goto out;
		}

		err = dio_clu_sync(clu);
		if (err) {
			KLOG(KL_ERR, "sync 0block err %d", err);
			dio_clu_put(clu);
			goto out;
		}
		dio_clu_put(clu);
	}
	atomic64_set(&sb->used_blocks, 0);

	err = 0;
out:
	return err;
}

static int ds_balloc_block_bm_bit(struct ds_sb *sb, u64 block,
	u64 *pblock, unsigned long *plong, long *pbit)
{
	u64 long_num;
	u32 bits_per_long = 8*sizeof(unsigned long);

	if (block >= sb->nr_blocks) {
		KLOG(KL_ERR, "block %llu out of sb blocks %llu",
			block, sb->nr_blocks);
		return -EINVAL;
	}

	long_num = ds_div(block, bits_per_long);

	*pbit = ds_mod(block, bits_per_long);
	*pblock = sb->bm_block + ds_div(long_num*sizeof(unsigned long),
					sb->bsize);
	*plong = ds_mod(long_num*sizeof(unsigned long), sb->bsize);

	return 0;
}

int ds_balloc_block_mark(struct ds_sb *sb, u64 block, int use)
{
	int err;
	long bit;
	unsigned long long_off;
	u64 bm_block;
	struct dio_cluster *clu;

	err = ds_balloc_block_bm_bit(sb, block, &bm_block, &long_off, &bit);
	if (err)
		return err;

	clu = dio_clu_get(sb->ddev, bm_block);
	if (!clu) {
		KLOG(KL_ERR, "cant read bm block %llu", bm_block);
		err = -EIO;
		goto out;
	}

	dio_clu_read_lock(clu);
	if (use) {
		BUG_ON(test_bit_le(bit, dio_clu_map(clu, long_off)));
		set_bit_le(bit, dio_clu_map(clu, long_off));
		atomic64_inc(&sb->used_blocks);
	} else {
		BUG_ON(!test_bit_le(bit, dio_clu_map(clu, long_off)));
		clear_bit_le(bit, dio_clu_map(clu, long_off));
		atomic64_dec(&sb->used_blocks);
	}
	dio_clu_read_unlock(clu);

	dio_clu_set_dirty(clu);
	err = dio_clu_sync(clu);
	if (err) {
		KLOG(KL_ERR, "cant sync block %llu", bm_block);
		goto cleanup;		
	}

	err = 0;

cleanup:
	dio_clu_put(clu);
out:
	up_write(&sb->rw_lock);
	return err;
}

int ds_balloc_block_free(struct ds_sb *sb, u64 block)
{
	return ds_balloc_block_mark(sb, block, 0);
}

static int ds_balloc_block_find_set_free_bit(struct ds_sb *sb,
	struct dio_cluster *clu, unsigned long *plong, long *pbit)
{
	int i, j;
	long bit;
	int pg_idx;
	int err;
	char *page;

	BUG_ON((clu->clu_size & (PAGE_SIZE - 1)));

	dio_clu_read_lock(clu);
	i = 0;
	for (pg_idx = 0; pg_idx < (clu->clu_size/PAGE_SIZE); pg_idx++) {
		page = dio_clu_map(clu, i);
		for (j = 0; j < PAGE_SIZE; j+= sizeof(unsigned long),
			i+= sizeof(unsigned long)) {
			unsigned long *addr = (unsigned long *)(page + j);
			if (*addr == (~((unsigned long)0)))
				continue;

			for (bit = 0; bit < (8*sizeof(unsigned long)); bit++) {
				if (!test_bit_le(bit, addr) &&
					!test_and_set_bit_le(bit, addr)) {

					atomic64_inc(&sb->used_blocks);
					dio_clu_set_dirty(clu);
					err = dio_clu_sync(clu);
					if (err) {
						KLOG(KL_ERR, "sync clu %llu err %d",
							clu->index, err);
						return err;
					}
					*plong = i;
					*pbit = bit;
					return 0;
				}
			}
		}
	}
	dio_clu_read_unlock(clu);

	return -ENOENT;
}

int ds_balloc_block_alloc(struct ds_sb *sb, u64 *pblock)
{
	struct dio_cluster *clu;
	u64 i;
	int err;
	long bit;
	unsigned long long_off;

	*pblock = 0;
	for (i = sb->bm_block; i < sb->bm_block + sb->bm_blocks; i++) {
		clu = dio_clu_get(sb->ddev, i);
		if (!clu) {
			KLOG(KL_ERR, "cant read block %llu", i);
			return -EIO;
		}

		err = ds_balloc_block_find_set_free_bit(sb, clu,
			&long_off, &bit);
		if (!err) {
			u64 block = ((i - sb->bm_block)*sb->bsize + long_off)*
				sizeof(unsigned long) + bit;

			KLOG(KL_DBG3, "alloc long_off %lu bit %u i %llu block %llu",
				long_off, bit , i, block);
			*pblock = block;
			dio_clu_put(clu);
			return 0;
		}
		dio_clu_put(clu);
	}

	return -ENOSPC;
}
