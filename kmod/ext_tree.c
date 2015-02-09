#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "ext_tree"

static struct kmem_cache *ext_tree_cachep;

static struct ext_tree *ext_tree_alloc(void)
{
	struct ext_tree *tree;

	tree = kmem_cache_alloc(ext_tree_cachep, GFP_NOIO);
	if (!tree)
		return NULL;
	memset(tree, 0, sizeof(*tree));
	atomic_set(&tree->ref, 1);
	return tree;
}

static void ext_tree_free(struct ext_tree *tree)
{
	kmem_cache_free(ext_tree_cachep, tree);
}

static void ext_tree_release(struct ext_tree *tree)
{
	btree_deref(tree->btree);
	ext_tree_free(tree);
}

void ext_tree_deref(struct ext_tree *tree)
{
	if (atomic_dec_and_test(&tree->ref))
		ext_tree_release(tree);	
}

struct ext_tree *ext_tree_create(struct ds_sb *sb, u64 block)
{
	struct ext_tree *tree;
	
	tree = ext_tree_alloc();
	if (!tree)
		return NULL;

	tree->sb = sb;
	tree->btree = btree_create(sb, block);
	if (!tree->btree)
		goto fail;

	return tree;
fail:
	ext_tree_free(tree);
	return NULL;
}

void ext_tree_stop(struct ext_tree *tree)
{
	tree->releasing = 1;
	btree_stop(tree->btree);
}

int ext_tree_init(void)
{
	int err;

	ext_tree_cachep = kmem_cache_create("ext_tree_cachep",
			sizeof(struct ext_tree), 0,
			SLAB_MEM_SPREAD, NULL);
	if (!ext_tree_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto out;
	}
	err = 0;
out:
	return err;
}

void ext_tree_finit(void)
{
	kmem_cache_destroy(ext_tree_cachep);
}

int ext_tree_ext_alloc(struct ext_tree *tree, u64 size, struct ext *ext)
{
	struct btree_node *node;
	int err;
	int index;

	btree_read_lock(tree->btree);
	node = btree_node_find_left_most(tree->btree->root, &index);
	if (node == NULL) {
		err = -ENOENT;
		goto unlock;
	}

	err = 0;
unlock:
	btree_read_unlock(tree->btree);
	return err;
}

void ext_tree_ext_free(struct ext_tree *tree, struct ext *ext)
{
	BUG();
}