// SPDX-License-Identifier: GPL-2.0
/*
 * Skip list v1: operations protected by one global spinlock.
 *
 * init and destroy require exclusive lifecycle access:
 * no worker may access the list while either function runs.
 *
 * Detached nodes are retained until destroy.
 */
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/spinlock.h>

#include "ds_ops.h"
#include "skiplist.h"

static struct sl_node *sl_head;
static struct sl_node *sl_retired;
static DEFINE_SPINLOCK(sl_lock);

/* Call from sleepable context, before taking a spinlock. */
static struct sl_node *sl_alloc_node(u64 key, unsigned int height)
{
	struct sl_node *node;
	size_t bytes;

	if (!height || height > SL_MAX_LEVEL)
		return NULL;

	bytes = sizeof(*node) + height * sizeof(node->next[0]);
	node = kzalloc(bytes, GFP_KERNEL);
	if (!node)
		return NULL;

	node->key = key;
	node->height = height;
	return node;
}

static int sl_init(void)
{
	if (sl_head)
		return -EBUSY;

	sl_head = sl_alloc_node(0, SL_MAX_LEVEL);
	if (!sl_head)
		return -ENOMEM;

	return 0;
}

/* Workers must have stopped and finished before this call. */
static void sl_destroy(void)
{
	struct sl_node *node, *next;
	unsigned int batch = 0;

	if (!sl_head)
		return;

	node = sl_head->next[0];
	while (node) {
		next = node->next[0];
		kfree(node);
		node = next;

		if (++batch == 1024) {
			cond_resched();
			batch = 0;
		}
	}

	/* Detached nodes are reclaimed only after all workers finish. */
	node = sl_retired;
	while (node) {
		next = node->next[0];
		kfree(node);
		node = next;

		if (++batch == 1024) {
			cond_resched();
			batch = 0;
		}
	}
	sl_retired = NULL;

	kfree(sl_head);
	sl_head = NULL;
}

/* Geometric height distribution with promotion probability 1/2. */
static unsigned int sl_random_height(void)
{
	u32 bits = get_random_u32();
	unsigned int height = 1;

	while (height < SL_MAX_LEVEL && (bits & 1U)) {
		height++;
		bits >>= 1;
	}

	return height;
}

/*
 * Return the first data node with node->key >= key, or NULL.
 *
 * The list must be initialized. The caller must hold the global
 * list lock throughout this call and any use of returned pointers.
 *
 * If update is non-NULL, it must have SL_MAX_LEVEL entries.
 * update[level] receives the last node with key < the target key,
 * or the head sentinel when no such data node exists on that level.
 * The target key must be a valid data key, excluding 0 and U64_MAX.
 */
static struct sl_node *sl_find_ge_locked(u64 key, struct sl_node **update)
{
	struct sl_node *node = sl_head;
	struct sl_node *next;
	int level;

	for (level = SL_MAX_LEVEL - 1; level >= 0; level--) {
		next = node->next[level];

		while (next && next->key < key) {
			node = next;
			next = node->next[level];
		}

		if (update)
			update[level] = node;
	}

	return node->next[0];
}

/* Called after successful init and before destroy, from worker context. */
static bool sl_search(u64 key)
{
	struct sl_node *node;
	bool found;

	if (!key || key == ~(u64)0)
		return false;

	spin_lock(&sl_lock);
	node = sl_find_ge_locked(key, NULL);
	found = node && node->key == key;
	spin_unlock(&sl_lock);

	return found;
}

/* Called from sleepable context after successful init. */
static int sl_insert(u64 key)
{
	struct sl_node *update[SL_MAX_LEVEL];
	struct sl_node *node, *found;
	unsigned int level;

	if (!key || key == ~(u64)0)
		return -EINVAL;

	node = sl_alloc_node(key, sl_random_height());
	if (!node)
		return -ENOMEM;

	spin_lock(&sl_lock);
	found = sl_find_ge_locked(key, update);

	if (found && found->key == key) {
		spin_unlock(&sl_lock);
		kfree(node);
		return -EEXIST;
	}

	for (level = 0; level < node->height; level++) {
		node->next[level] = update[level]->next[level];
		update[level]->next[level] = node;
	}

	spin_unlock(&sl_lock);
	return 0;
}

/* Called after successful init and before destroy. */
static int sl_remove(u64 key)
{
	struct sl_node *update[SL_MAX_LEVEL];
	struct sl_node *node;
	unsigned int level;

	if (!key || key == ~(u64)0)
		return -EINVAL;

	spin_lock(&sl_lock);
	node = sl_find_ge_locked(key, update);

	if (!node || node->key != key) {
		spin_unlock(&sl_lock);
		return -ENOENT;
	}

	for (level = 0; level < node->height; level++)
		update[level]->next[level] = node->next[level];

	/*
	 * V1 only: no reader retains node pointers outside sl_lock.
	 * Reuse next[0] only after unlinking the node from every level.
	 */
	node->next[0] = sl_retired;
	sl_retired = node;

	spin_unlock(&sl_lock);
	return 0;
}

/* Count keys in the inclusive interval while holding the list lock. */
static u64 sl_range(u64 lo, u64 hi)
{
	struct sl_node *node;
	u64 count = 0;

	if (lo > hi || hi == 0 || lo == ~(u64)0)
		return 0;

	if (lo == 0)
		lo = 1;

	spin_lock(&sl_lock);
	node = sl_find_ge_locked(lo, NULL);

	while (node && node->key <= hi) {
		count++;
		node = node->next[0];
	}

	spin_unlock(&sl_lock);
	return count;
}

struct ds_ops bench_ds = {
	.name = "sl1",
	.init = sl_init,
	.insert = sl_insert,
	.search = sl_search,
	.remove = sl_remove,
	.range = sl_range,
	.destroy = sl_destroy,
};
