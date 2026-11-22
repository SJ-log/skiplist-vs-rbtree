# skiplist-vs-rbtree

중앙대 「리눅스 시스템 응용 설계」 텀프로젝트. 커널 rbtree(전역 락)와 직접 구현한 skip list(노드별 락)를
커널 모듈로 만들어 멀티스레드 처리량을 비교한다. 핵심 주장: 락을 잘게 쪼개면 스레드가 늘수록 빨라진다.

## 먼저 읽을 것

- `docs/index.html` — 최신 계획을 담은 팀 공유 페이지 (프로젝트 설명, 가설, 실험, 역할, 병목)
- `docs/part-A-harness.md` — A(강성준) 파트 인수인계: 측정 틀, rbtree, PM 할 일
- `TEAM_GUIDE.md` — 초기 계획. 아래 "바뀐 점"과 다르면 이 파일이 우선한다

## 과제 요건 (Course Overview 슬라이드 9·11)

- 유형: Improving performance of kernel source code — Data structure
- 커널 자료구조와 비교해 insert / delete / traverse 성능을 개선해야 한다
- 제출: 프로젝트 설명이 담긴 슬라이드 + 소스 코드, 발표는 팀당 10분

## TEAM_GUIDE 이후 바뀐 점

- 구현 5개: rbtree+spinlock, rbtree+rwlock (A) / skip v1 락 1개 (B) / maple tree (D, v3 대신) / skip v2 lazy·노드별 락 (C, 핵심). v3 RCU는 여유 있을 때만 D가
- `ds_ops`에 `u64 (*range)(u64 lo, u64 hi)` 추가 예정 — [lo, hi] 안의 키 개수. 첫 회의에서 확정하기 전까지는 제안
- 작업 비율(탐색·삽입·삭제): 100·0·0 / 90·5·5 / 50·25·25 / 0·50·50. "삽입만" 조건은 뺐다 (키가 금방 다 차서 탐색을 재게 됨)
- 스레드 수 1·2·4·8 (VM의 vCPU 수까지)
- 범위 탐색 실험 추가: 스캔 스레드 1 + 쓰기 스레드 1·2·4, scan_len 100·1,000
- 분석용 카운터: 연산당 이동 노드 수, v2 검증 실패 재시도 횟수. 컴파일 옵션으로만 켠다
- 날짜 기반 일정 대신 의존 순서로 관리한다. 병목은 `docs/index.html`의 "병목" 섹션 참고

## 역할

| 역할 | 담당 | 맡는 일 |
|---|---|---|
| A · PM | 강성준 | 측정 틀(harness), rbtree spin/rw, 통합 |
| B | 이은찬 | skip v1, 정합성 검사·스트레스 테스트, v2 페어 |
| C | 이우신 | skip v2 (lazy skip list, 노드별 락) |
| D | 종주원 | 실험 스크립트·그래프·발표 총괄, maple tree |

리뷰 짝: 강성준 ↔ 이은찬, 이우신 ↔ 종주원

## 구조

```
include/ds_ops.h  공통 인터페이스. 각 구현은 struct ds_ops bench_ds 를 하나 정의한다
harness/          측정 틀 (A)
ds/               구현: rbtree_lock.c, rbtree_rw.c, skiplist_v1.c, skiplist_v2.c, maple.c
tests/            정합성 검사 (B)
scripts/          실험 실행·그래프 (D)
results/          측정 결과 CSV
docs/             팀 공유 페이지, 역할별 인수인계
```

- 모듈 하나 = `harness.o` + 구현 하나 (Makefile의 `obj-m` + `<모듈>-objs` 쌍)
- 빌드 `make` (KDIR 기본값 `/lib/modules/$(uname -r)/build`)
- 실행 `sudo insmod bench_xx.ko nthreads=4 ...` → `dmesg | grep RESULT` → `sudo rmmod bench_xx`

## 개발 환경 주의

- 빌드·실행은 각자의 Ubuntu VM에서 한다 (TEAM_GUIDE 기준 ARM64). Claude 클라우드 세션에는 대상 커널 헤더가 없고 insmod도 못 하므로, 거기서는 코드 작성·리뷰까지만 하고 빌드 결과는 VM에서 받아 온다
- 최종 측정은 지정한 VM 한 대에서만 한다

## 코딩 규칙

- 커널 코딩 스타일: 탭 들여쓰기, snake_case, `pr_info`/`pr_err`, `scripts/checkpatch.pl -f <파일>`
- 스핀락 안에서 잠들 수 있는 호출 금지 (`kmalloc(GFP_KERNEL)` 포함) → 락 잡기 전에 할당하고, 안 쓰게 되면 락을 푼 뒤 `kfree`
- 동시 자료구조에서 삭제한 노드는 바로 `kfree`하지 않는다 → 지연 해제 목록에 모았다가 `destroy`에서 일괄 해제
- 반복문 안에서 `pr_info` 금지. 결과는 요약 한 줄
- 긴 루프는 락 밖에서 `cond_resched()`
- 스레드별 카운터는 `____cacheline_aligned_in_smp`로 false sharing 방지
- 락은 항상 아래층 → 위층 순서로 잡는다 (skip list)

## Git 규칙

- `main` 직접 push 금지. 브랜치: `feat/harness`, `feat/sl-v1`, `feat/sl-v2`, `feat/maple`, `feat/scripts`, `docs/...`
- PR → 리뷰 짝 approve → merge (제안: approve 받으면 작성자가 merge, PM은 main 빌드만 확인)
- 커밋 메시지 `영역: 무엇을` (예: `harness: add start barrier`)
- 빌드 산출물(`*.o`, `*.ko` 등)은 커밋하지 않는다
