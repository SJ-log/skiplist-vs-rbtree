/* SPDX-License-Identifier: GPL-2.0 */
#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <linux/types.h>

/* Number of levels: valid indices are 0 through 19. */
#define SL_MAX_LEVEL 20

/*
 * Shared node layout proposal for skip list v1 and v2.
 *
 * height is the number of levels, not the highest level index.
 * A node with height 3 has next[0], next[1], and next[2].
 *
 * Allocate space for height pointers in the flexible array.
 * sizeof(struct sl_node) alone does not include that space.
 *
 * V2 synchronization and node retirement fields will be agreed
 * with the V2 owner before extending this layout.
 */
struct sl_node {
	u64 key;
	unsigned int height;
	struct sl_node *next[];
};

#endif /* SKIPLIST_H */
