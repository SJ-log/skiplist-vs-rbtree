// SPDX-License-Identifier: GPL-2.0
/*
 * Structural checks for a quiescent skip list.
 * Call after all workers stop, without holding a spinlock.
 * Pointers must refer to valid allocations with sufficient storage.
 */
#include <linux/errno.h>
#include <linux/sched.h>

#include "skiplist_validate.h"

int sl_validate(const struct sl_node *head, u64 expected_count)
{
	const struct sl_node *node, *lower, *upper;
	u64 count = 0, previous = 0, steps = 0;
	unsigned int level;

	if (!head || head->height != SL_MAX_LEVEL || head->key != 0)
		return -EINVAL;

	/* Validate level 0 before following any upper-level links. */
	for (node = head->next[0]; node; node = node->next[0]) {
		if (!node->height || node->height > SL_MAX_LEVEL)
			return -EINVAL;
		if (node->key <= previous || node->key == ~(u64)0)
			return -EINVAL;
		if (count == expected_count)
			return -EINVAL;

		previous = node->key;
		count++;
		if (!(++steps & 1023))
			cond_resched();
	}

	if (count != expected_count)
		return -EINVAL;

	/*
	 * Each upper level must contain exactly the nodes from the
	 * preceding level whose height includes this level.
	 * Compare addresses before dereferencing an upper-level node.
	 */
	for (level = 1; level < SL_MAX_LEVEL; level++) {
		lower = head->next[level - 1];
		upper = head->next[level];

		while (lower) {
			if (lower->height > level) {
				if (upper != lower)
					return -EINVAL;
				upper = upper->next[level];
			}

			lower = lower->next[level - 1];
			if (!(++steps & 1023))
				cond_resched();
		}

		if (upper)
			return -EINVAL;
	}

	return 0;
}
