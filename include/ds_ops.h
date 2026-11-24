/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ds_ops.h - 모든 구현이 맞춰야 하는 공통 인터페이스
 *
 * 2026-11-24 첫 회의에서 확정했다. 바꾸려면 전원이 합의한다.
 *
 * 규칙
 *  - 반환값은 커널 관례를 따른다. 성공 0, 실패 -errno.
 *  - 사용자 키는 1 이상 2 * nkeys 이하다.
 *    0과 U64_MAX는 구현이 센티넬로 쓰도록 비워 둔다.
 *    측정 틀은 이 두 값을 넘기지 않는다.
 *  - init과 destroy는 한 스레드에서만 호출된다.
 *    나머지는 여러 스레드가 동시에 호출한다.
 */
#ifndef _DS_OPS_H
#define _DS_OPS_H

#include <linux/types.h>

/*
 * 스레드별 데이터는 이 크기로 정렬해 false sharing을 막는다.
 *   struct foo { ... } __aligned(BENCH_CACHE_ALIGN);
 * Apple Silicon의 캐시 라인은 128바이트라서
 * ____cacheline_aligned_in_smp (arm64에서 64바이트)로는 부족하다.
 */
#define BENCH_CACHE_ALIGN	128

struct ds_ops {
	const char *name;

	/* 빈 자료구조를 만든다. 0 | -errno */
	int  (*init)(void);

	/* key를 넣는다. 0 | -EEXIST (이미 있음) | -ENOMEM */
	int  (*insert)(u64 key);

	/* key가 있으면 true */
	bool (*search)(u64 key);

	/* key를 지운다. 0 | -ENOENT (없음) */
	int  (*remove)(u64 key);

	/*
	 * lo 이상 hi 이하인 키의 개수.
	 * 세는 동안 다른 스레드가 넣거나 지우면 근사치여도 된다.
	 */
	u64  (*range)(u64 lo, u64 hi);

	/* 남은 노드를 모두 해제한다 */
	void (*destroy)(void);
};

/* ds/ 아래 각 구현 파일이 하나씩 정의하고, harness가 이것을 사용한다 */
extern struct ds_ops bench_ds;

#endif /* _DS_OPS_H */
