// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/kthread.h>

#include "skiplist_validate.h"

/* Include the actual V1 implementation for private-state validation. */
#include "../ds/skiplist_v1.c"

#define TEST_THREADS 4
#define TEST_KEYS 256

struct test_worker {
	struct completion done;
	unsigned int success;
	unsigned int expected_error;
	unsigned int unexpected_error;
};

static struct test_worker test_workers[TEST_THREADS];
static DECLARE_COMPLETION(test_gate);
static bool test_deleting;

static int test_worker_fn(void *arg)
{
	struct test_worker *worker = arg;
	unsigned int key;
	int ret, expected;

	wait_for_completion(&test_gate);
	expected = test_deleting ? -ENOENT : -EEXIST;

	for (key = 1; key <= TEST_KEYS; key++) {
		if (test_deleting)
			ret = bench_ds.remove(key);
		else
			ret = bench_ds.insert(key);

		if (!ret)
			worker->success++;
		else if (ret == expected)
			worker->expected_error++;
		else
			worker->unexpected_error++;

		if (!(key & 31))
			cond_resched();
	}

	complete_and_exit(&worker->done, 0);
}

static int run_phase(const char *name, bool deleting)
{
	unsigned int i, created = 0;
	unsigned int success = 0, expected_error = 0, unexpected_error = 0;
	unsigned int missing_or_extra = 0;
	u64 expected_count, range_count;
	struct task_struct *task;
	int ret = 0, structure;

	test_deleting = deleting;
	reinit_completion(&test_gate);

	for (i = 0; i < TEST_THREADS; i++) {
		struct test_worker *worker = &test_workers[i];

		worker->success = 0;
		worker->expected_error = 0;
		worker->unexpected_error = 0;
		init_completion(&worker->done);

		task = kthread_run(test_worker_fn, worker, "sl1-test/%u", i);
		if (IS_ERR(task)) {
			ret = PTR_ERR(task);
			break;
		}
		created++;
	}

	complete_all(&test_gate);
	for (i = 0; i < created; i++)
		wait_for_completion(&test_workers[i].done);

	if (ret) {
		pr_err("sl1_concurrent: worker creation failed: %d\n", ret);
		return ret;
	}

	for (i = 0; i < TEST_THREADS; i++) {
		success += test_workers[i].success;
		expected_error += test_workers[i].expected_error;
		unexpected_error += test_workers[i].unexpected_error;
	}

	expected_count = deleting ? 0 : TEST_KEYS;
	structure = sl_validate(sl_head, expected_count);
	range_count = bench_ds.range(1, TEST_KEYS);

	for (i = 1; i <= TEST_KEYS; i++) {
		if (bench_ds.search(i) != !deleting)
			missing_or_extra++;
	}

	pr_info("sl1_concurrent: %s success=%u expected_error=%u unexpected=%u\n",
		name, success, expected_error, unexpected_error);
	pr_info("sl1_concurrent: structure=%d range=%llu key_errors=%u\n",
		structure, (unsigned long long)range_count, missing_or_extra);

	if (success != TEST_KEYS ||
		expected_error != TEST_KEYS * (TEST_THREADS - 1) ||
		unexpected_error || structure || missing_or_extra ||
		range_count != expected_count)
		return -EINVAL;

	return 0;
}

static int __init sl1_concurrent_init(void)
{
	int ret;

	ret = bench_ds.init();
	if (ret)
		return ret;

	ret = run_phase("insert", false);
	if (!ret)
		ret = run_phase("remove", true);
	if (!ret)
		ret = run_phase("reinsert", false);

	bench_ds.destroy();

	if (sl_head || sl_retired)
		ret = -EINVAL;

	if (ret)
		pr_err("sl1_concurrent: FAIL ret=%d\n", ret);
	else
		pr_info("sl1_concurrent: PASS phases=3 threads=4 keys=256\n");

	return ret;
}

static void __exit sl1_concurrent_exit(void)
{
}

module_init(sl1_concurrent_init);
module_exit(sl1_concurrent_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Skip list V1 concurrent operation tests");
