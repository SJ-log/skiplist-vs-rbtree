// SPDX-License-Identifier: GPL-2.0
/*
 * dummy.c - 아무 일도 하지 않는 구현
 *
 * 측정 틀 자체의 비용(키 생성, 함수 호출)을 재고,
 * 실험 스크립트를 미리 연결해 보는 데 쓴다.
 */
#include <linux/types.h>

#include "ds_ops.h"

static int dummy_init(void)
{
	return 0;
}

static int dummy_insert(u64 key)
{
	return 0;
}

static bool dummy_search(u64 key)
{
	return false;
}

static int dummy_remove(u64 key)
{
	return 0;
}

static void dummy_destroy(void)
{
}

struct ds_ops bench_ds = {
	.name    = "dummy",
	.init    = dummy_init,
	.insert  = dummy_insert,
	.search  = dummy_search,
	.remove  = dummy_remove,
	.destroy = dummy_destroy,
};
