/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ds_ops.h - 모든 구현이 맞춰야 하는 공통 인터페이스
 *
 * 초안: Day 1에 4명이 함께 확정한다.
 * 정할 것: 반환값 규칙(성공 0 / 실패 -errno?), 중복 키 처리, 키 타입
 */
#ifndef _DS_OPS_H
#define _DS_OPS_H

#include <linux/types.h>

struct ds_ops {
	const char *name;
	int  (*init)(void);
	int  (*insert)(u64 key);
	bool (*search)(u64 key);
	int  (*remove)(u64 key);
	void (*destroy)(void);
};

/* ds/ 아래 각 구현 파일이 하나씩 정의하고, harness가 이것을 사용한다 */
extern struct ds_ops bench_ds;

#endif /* _DS_OPS_H */
