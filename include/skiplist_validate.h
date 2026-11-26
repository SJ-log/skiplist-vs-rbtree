/* SPDX-License-Identifier: GPL-2.0 */
#ifndef SKIPLIST_VALIDATE_H
#define SKIPLIST_VALIDATE_H

#include "skiplist.h"

/*
 * Validate a quiescent skip list: no concurrent readers or writers.
 *
 * Layout contract:
 * - head is a sentinel with height == SL_MAX_LEVEL.
 * - Every level ends with NULL; there is no tail sentinel.
 * - Data nodes have height in [1, SL_MAX_LEVEL].
 * - All reachable data nodes are fully linked and live.
 *
 * Checks:
 * - Keys are strictly increasing on every level.
 * - Each upper level is a subsequence of the level below it.
 * - The level-0 data-node count equals expected_count.
 *
 * expected_count excludes head and any detached retired nodes.
 * The caller must provide valid, allocated nodes.
 *
 * Return 0 on success or a negative errno on failure.
 * This function does not modify the list or acquire locks.
 */
int sl_validate(const struct sl_node *head, u64 expected_count);

#endif /* SKIPLIST_VALIDATE_H */
