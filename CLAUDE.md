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

- 구현 5개: rbtree+spinlock, rbtree+rwlock (A) / skip v1 락 1개 (B) / xarray (D, v3 대신) / skip v2 lazy·노드별 락 (C, 핵심). v3 RCU는 여유 있을 때만 D가
- `ds_ops`에 `u64 (*range)(u64 lo, u64 hi)` 추가 예정 — [lo, hi] 안의 키 개수. 첫 회의에서 확정하기 전까지는 제안
- 작업 비율(탐색·삽입·삭제): 100·0·0 / 90·5·5 / 50·25·25 / 0·50·50. "삽입만" 조건은 뺐다 (키가 금방 다 차서 탐색을 재게 됨)
- 스레드 수 1·2·4 (실험 VM의 vCPU가 4개. 8개로 늘릴 수 있으면 8도 잰다)
- 범위 탐색 실험 추가: 스캔 스레드 1 + 쓰기 스레드 1·2·3, scan_len 100·1,000
- 실험 커널은 5.4다 (강성준 VM: 5.4.214, aarch64, vCPU 4, Ubuntu 20.04). 팀원 VM도 같은지는 첫 회의에서 확인한다
- 커널 5.4 때문에 바꾼 것 (제안, 첫 회의에서 확정): maple tree(6.1부터) → xarray, 스레드 수 1·2·4·8 → 1·2·4, 범위 탐색의 쓰기 스레드 1·2·4 → 1·2·3
- 분석용 카운터: 연산당 이동 노드 수, v2 검증 실패 재시도 횟수. 컴파일 옵션으로만 켠다
- PR 확인은 리뷰 짝 대신 강성준(PM)이 전부 맡는다. 강성준 본인의 PR은 리뷰 없이 직접 merge한다
- 날짜 기반 일정 대신 의존 순서로 관리한다. 병목은 `docs/index.html`의 "병목" 섹션 참고

## 역할

| 역할 | 담당 | 맡는 일 |
|---|---|---|
| A · PM | 강성준 | 측정 틀(harness), rbtree spin/rw, 통합 |
| B | 이은찬 | skip v1, 정합성 검사·스트레스 테스트, v2 페어 |
| C | 이우신 | skip v2 (lazy skip list, 노드별 락) |
| D | 종주원 | 실험 스크립트·그래프·발표 총괄, xarray |

PR 확인: 모든 PR은 강성준이 확인·merge. 강성준 본인의 PR은 리뷰 없이 직접 merge한다.
백업 짝: 강성준 ↔ 이은찬, 이우신 ↔ 종주원 — 서로의 파트를 설명할 수 있게 유지

## 구조

```
include/ds_ops.h  공통 인터페이스. 각 구현은 struct ds_ops bench_ds 를 하나 정의한다
harness/          측정 틀 (A)
ds/               구현: rbtree_lock.c, rbtree_rw.c, skiplist_v1.c, skiplist_v2.c, xarray.c
tests/            정합성 검사 (B)
scripts/          실험 실행·그래프 (D)
results/          측정 결과 CSV
docs/             팀 공유 페이지, 역할별 인수인계
```

- 모듈 하나 = `harness.o` + 구현 하나 (Makefile의 `obj-m` + `<모듈>-objs` 쌍)
- 빌드 `make` (KDIR 기본값 `/lib/modules/$(uname -r)/build`)
- 실행 `sudo insmod bench_xx.ko nthreads=4 ...` → `dmesg | grep RESULT` → `sudo rmmod bench_xx`
- 결과 형식 `RESULT,ds,threads,nkeys,search,insert,remove,scan_len,ops,scan_ops,elapsed_ns`. 처리량은 `ops / elapsed_ns × 1e9`로 구한다 (실제 경과 시간은 3초보다 조금 길게 나온다)

## 개발 환경 주의

- 빌드·실행은 각자의 Ubuntu VM에서 한다 (TEAM_GUIDE 기준 ARM64). Claude 클라우드 세션에는 대상 커널 헤더가 없고 insmod도 못 하므로, 거기서는 코드 작성·리뷰까지만 하고 빌드 결과는 VM에서 받아 온다
- 최종 측정은 지정한 VM 한 대에서만 한다
- 커널 5.4 기준으로 쓴다. 5.4에 없는 것: maple tree(6.1부터), `kthread_complete_and_exit`(5.17부터. 5.4에서는 `complete_and_exit`). 새 API를 쓰기 전에 5.4에 있는지 확인한다

## 코딩 규칙

- 커널 코딩 스타일: 탭 들여쓰기, snake_case, `pr_info`/`pr_err`, `scripts/checkpatch.pl -f <파일>`
- 스핀락 안에서 잠들 수 있는 호출 금지 (`kmalloc(GFP_KERNEL)` 포함) → 락 잡기 전에 할당하고, 안 쓰게 되면 락을 푼 뒤 `kfree`
- 동시 자료구조에서 삭제한 노드는 바로 `kfree`하지 않는다 → 지연 해제 목록에 모았다가 `destroy`에서 일괄 해제
- 반복문 안에서 `pr_info` 금지. 결과는 요약 한 줄
- 긴 루프는 락 밖에서 `cond_resched()`
- 스레드별 카운터는 128바이트로 정렬해 false sharing을 막는다 (`__aligned(128)`). Apple Silicon의 캐시 라인이 128바이트라서 `____cacheline_aligned_in_smp`(arm64에서 64바이트)로는 부족하다. 더미 4스레드에서 3.16배 → 3.79배로 확인했다
- 락은 항상 아래층 → 위층 순서로 잡는다 (skip list)

## Git 규칙

- `main` 직접 push 금지. 브랜치: `feat/harness`, `feat/sl-v1`, `feat/sl-v2`, `feat/xarray`, `feat/scripts`, `docs/...`
- PR → 강성준(PM)이 확인·approve → merge (`.github/CODEOWNERS`로 리뷰 요청이 자동으로 간다). 강성준 본인의 PR은 리뷰 없이 직접 merge한다
- PR은 단계 하나씩 작게 올리고 `.github/pull_request_template.md`의 체크리스트를 채운다
- 커밋 메시지 `영역: 무엇을` (예: `harness: add start barrier`)
- 빌드 산출물(`*.o`, `*.ko` 등)은 커밋하지 않는다
