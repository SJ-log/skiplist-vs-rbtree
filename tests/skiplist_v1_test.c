// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>

#include "skiplist_validate.h"

/* Test the actual implementation and inspect its private state. */
#include "../ds/skiplist_v1.c"

static unsigned int passed;
static unsigned int failed;

static void expect_value(const char *name, s64 actual, s64 expected)
{
	if (actual == expected) {
		passed++;
		return;
	}

	failed++;
	pr_err("sl1_test: %s: got=%lld expected=%lld\n",
	       name, (long long)actual, (long long)expected);
}

static int __init sl1_test_init(void)
{
	int ret;

	ret = bench_ds.init();
	if (ret)
		return ret;

	expect_value("empty structure", sl_validate(sl_head, 0), 0);
	expect_value("empty search", bench_ds.search(10), false);
	expect_value("empty range", bench_ds.range(1, 100), 0);
	expect_value("empty remove", bench_ds.remove(10), -ENOENT);

	expect_value("insert 30", bench_ds.insert(30), 0);
	expect_value("insert 10", bench_ds.insert(10), 0);
	expect_value("insert 20", bench_ds.insert(20), 0);
	expect_value("three-node structure", sl_validate(sl_head, 3), 0);
	expect_value("duplicate 20", bench_ds.insert(20), -EEXIST);
	expect_value("duplicate preserves structure", sl_validate(sl_head, 3), 0);

	expect_value("search 10", bench_ds.search(10), true);
	expect_value("search 20", bench_ds.search(20), true);
	expect_value("search 30", bench_ds.search(30), true);
	expect_value("search absent", bench_ds.search(15), false);

	expect_value("range inclusive", bench_ds.range(10, 30), 3);
	expect_value("range single key", bench_ds.range(20, 20), 1);
	expect_value("range gap", bench_ds.range(11, 19), 0);
	expect_value("range reversed", bench_ds.range(30, 10), 0);
	expect_value("range full", bench_ds.range(0, ~(u64)0), 3);
	expect_value("range zero", bench_ds.range(0, 0), 0);
	expect_value("range max", bench_ds.range(~(u64)0, ~(u64)0), 0);

	expect_value("reject insert zero", bench_ds.insert(0), -EINVAL);
	expect_value("reject insert max", bench_ds.insert(~(u64)0), -EINVAL);
	expect_value("search zero", bench_ds.search(0), false);
	expect_value("search max", bench_ds.search(~(u64)0), false);
	expect_value("reject remove zero", bench_ds.remove(0), -EINVAL);
	expect_value("reject remove max", bench_ds.remove(~(u64)0), -EINVAL);

	expect_value("remove middle", bench_ds.remove(20), 0);
	expect_value("removed key absent", bench_ds.search(20), false);
	expect_value("remove again", bench_ds.remove(20), -ENOENT);
	expect_value("two-node structure", sl_validate(sl_head, 2), 0);
	expect_value("range after remove", bench_ds.range(10, 30), 2);
	expect_value("retirement recorded", sl_retired != NULL, true);

	expect_value("reinsert 20", bench_ds.insert(20), 0);
	expect_value("reinserted search", bench_ds.search(20), true);
	expect_value("reinserted structure", sl_validate(sl_head, 3), 0);

	expect_value("remove first", bench_ds.remove(10), 0);
	expect_value("remove last", bench_ds.remove(30), 0);
	expect_value("remove remaining", bench_ds.remove(20), 0);
	expect_value("empty again", sl_validate(sl_head, 0), 0);
	expect_value("empty range again", bench_ds.range(0, ~(u64)0), 0);

	/* Leave a live node alongside retired nodes for destroy(). */
	expect_value("insert before destroy", bench_ds.insert(40), 0);
	expect_value("live and retired structure", sl_validate(sl_head, 1), 0);
	bench_ds.destroy();
	expect_value("head reset", sl_head == NULL, true);
	expect_value("retired reset", sl_retired == NULL, true);

	ret = bench_ds.init();
	expect_value("reinitialize", ret, 0);
	if (!ret) {
		expect_value("reinitialized structure", sl_validate(sl_head, 0), 0);
		expect_value("insert after reinit", bench_ds.insert(50), 0);
		expect_value("search after reinit", bench_ds.search(50), true);
		expect_value("structure after reinit", sl_validate(sl_head, 1), 0);
		bench_ds.destroy();
	}

	pr_info("sl1_test: passed=%u failed=%u\n", passed, failed);
	return failed ? -EINVAL : 0;
}

static void __exit sl1_test_exit(void)
{
}

module_init(sl1_test_init);
module_exit(sl1_test_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Skip list V1 operation and structure tests");
