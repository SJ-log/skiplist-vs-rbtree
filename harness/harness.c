// SPDX-License-Identifier: GPL-2.0
/*
 * harness.c - 측정 틀
 *
 * 구현 하나(bench_ds)와 함께 링크되어 모듈 하나가 된다.
 * insmod 하면 측정을 한 번 돌리고 결과를 한 줄로 남긴다.
 *
 *   sudo insmod bench_xx.ko nthreads=4 nkeys=100000 duration_ms=3000
 *   dmesg | grep RESULT
 *   sudo rmmod bench_xx
 *
 * 출력 형식:
 *   RESULT,ds,threads,nkeys,search,insert,remove,scan_len,
 *          ops,scan_ops,elapsed_ns
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/cache.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/hash.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/random.h>
#include <linux/sched.h>

#include "ds_ops.h"

#define BENCH_MAX_THREADS	64
#define BENCH_MAX_NKEYS		(1U << 24)
#define BENCH_MAX_DURATION_MS	60000
#define BENCH_SEED		0x5eedULL
#define BENCH_RESCHED_MASK	1023	/* 1024연산마다 CPU 양보 */

static unsigned int nthreads = 1;
module_param(nthreads, uint, 0444);
MODULE_PARM_DESC(nthreads, "number of worker threads");

static unsigned int nkeys = 100000;
module_param(nkeys, uint, 0444);
MODULE_PARM_DESC(nkeys, "number of keys to prefill");

static unsigned int duration_ms = 3000;
module_param(duration_ms, uint, 0444);
MODULE_PARM_DESC(duration_ms, "measurement time in milliseconds");

/*
 * 워커끼리 같은 캐시 라인을 쓰지 않도록 정렬한다.
 * ____cacheline_aligned_in_smp 는 arm64에서 64바이트인데
 * Apple Silicon의 캐시 라인은 128바이트라서 직접 지정한다.
 */
#define BENCH_CACHE_ALIGN	128

struct bench_worker {
	struct rnd_state rnd;
	struct completion done;
	u64 ops;
} __aligned(BENCH_CACHE_ALIGN);

static struct bench_worker workers[BENCH_MAX_THREADS];

static DECLARE_COMPLETION(all_ready);	/* 워커 전원이 출발선에 섰다 */
static DECLARE_COMPLETION(start_gate);	/* 출발 신호 */
static atomic_t nready = ATOMIC_INIT(0);
static bool stop_flag __read_mostly;

static int worker_fn(void *arg)
{
	struct bench_worker *w = arg;
	u32 range = 2 * nkeys;
	u64 ops = 0;

	if (atomic_inc_return(&nready) == (int)nthreads)
		complete(&all_ready);
	wait_for_completion(&start_gate);

	while (!READ_ONCE(stop_flag)) {
		u64 key = 1 + prandom_u32_state(&w->rnd) % range;

		bench_ds.search(key);
		ops++;
		/* 이게 없으면 정지 신호를 줄 스레드가 CPU를 못 얻는다 */
		if (!(ops & BENCH_RESCHED_MASK))
			cond_resched();
	}

	w->ops = ops;
	/* 커널 5.17부터는 kthread_complete_and_exit() */
	complete_and_exit(&w->done, 0);
}

/* 워커 n개를 멈추고 전원이 끝날 때까지 기다린다 */
static void __init stop_workers(unsigned int n)
{
	unsigned int i;

	WRITE_ONCE(stop_flag, true);
	complete_all(&start_gate);	/* 아직 출발 전이면 깨운다 */
	for (i = 0; i < n; i++)
		wait_for_completion(&workers[i].done);
}

static int __init start_workers(void)
{
	unsigned int i;

	for (i = 0; i < nthreads; i++) {
		struct bench_worker *w = &workers[i];
		struct task_struct *task;

		init_completion(&w->done);
		prandom_seed_state(&w->rnd,
				   BENCH_SEED + (i + 1) * GOLDEN_RATIO_64);

		task = kthread_create(worker_fn, w, "bench/%u", i);
		if (IS_ERR(task)) {
			pr_err("bench: cannot create worker %u\n", i);
			stop_workers(i);
			return PTR_ERR(task);
		}
		wake_up_process(task);
	}
	return 0;
}

/*
 * 짝수 키 2, 4, ..., 2 * nkeys 를 넣어 정확히 nkeys개로 시작한다.
 * 2단계에서 무작위 키로 채우도록 바꾼다.
 */
static int __init prefill(void)
{
	unsigned int i;
	int ret;

	for (i = 1; i <= nkeys; i++) {
		ret = bench_ds.insert(2ULL * i);
		if (ret) {
			pr_err("bench: prefill failed at key %llu: %d\n",
			       2ULL * i, ret);
			return ret;
		}
		if (!(i & BENCH_RESCHED_MASK))
			cond_resched();
	}
	return 0;
}

static int __init check_params(void)
{
	if (nthreads < 1 || nthreads > BENCH_MAX_THREADS) {
		pr_err("bench: nthreads must be 1..%d\n", BENCH_MAX_THREADS);
		return -EINVAL;
	}
	if (nkeys < 1 || nkeys > BENCH_MAX_NKEYS) {
		pr_err("bench: nkeys must be 1..%u\n", BENCH_MAX_NKEYS);
		return -EINVAL;
	}
	if (duration_ms < 1 || duration_ms > BENCH_MAX_DURATION_MS) {
		pr_err("bench: duration_ms must be 1..%d\n",
		       BENCH_MAX_DURATION_MS);
		return -EINVAL;
	}
	return 0;
}

static int __init bench_init(void)
{
	u64 t0, t1, ops = 0;
	unsigned int i;
	int ret;

	ret = check_params();
	if (ret)
		return ret;

	ret = bench_ds.init();
	if (ret) {
		pr_err("bench: %s: init failed: %d\n", bench_ds.name, ret);
		return ret;
	}

	ret = prefill();
	if (ret)
		goto err_destroy;

	ret = start_workers();
	if (ret)
		goto err_destroy;

	/* 전원이 출발선에 선 뒤에 시계를 누르고 동시에 출발시킨다 */
	wait_for_completion(&all_ready);
	t0 = ktime_get_ns();
	complete_all(&start_gate);

	msleep(duration_ms);

	stop_workers(nthreads);
	t1 = ktime_get_ns();

	for (i = 0; i < nthreads; i++)
		ops += workers[i].ops;

	/* 비율(100,0,0)과 scan_len, scan_ops 칸은 이후 단계에서 채운다 */
	pr_info("RESULT,%s,%u,%u,100,0,0,0,%llu,0,%llu\n",
		bench_ds.name, nthreads, nkeys, ops, t1 - t0);
	return 0;

err_destroy:
	bench_ds.destroy();
	return ret;
}

static void __exit bench_exit(void)
{
	bench_ds.destroy();
}

module_init(bench_init);
module_exit(bench_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("skip list vs rbtree benchmark harness");
