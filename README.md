# skiplist-vs-rbtree

중앙대 「리눅스 시스템 응용 설계」 텀프로젝트 — **Improving performance of kernel source code (data structure)**

> 락을 잘게 쪼개면, 스레드가 늘수록 빨라진다.

커널의 `rbtree` + 전역 락(global lock)과, 직접 구현한 skip list + 노드별 락(fine-grained lock)을
커널 모듈로 만들어 멀티스레드 처리량(throughput)과 지연(latency)을 비교한다.

**최신 계획은 팀 공유 페이지 [`docs/index.html`](docs/index.html)에 있다** (내려받아 브라우저로 열기).
역할별 인수인계: [`docs/part-A-harness.md`](docs/part-A-harness.md) (A · 측정 틀 + rbtree).

## 팀

| 역할 | 이름 | 담당 |
|---|---|---|
| A · PM | 강성준 | 전체 설계·프로젝트 관리, 측정 틀, rbtree baseline |
| B | 이은찬 | skip list v1, 정합성 검증 |
| C | 이우신 | skip list v2 (노드별 락) |
| D | 종주원 | 실험 자동화, 분석, 발표 총괄 |

역할별 상세 할 일은 [TEAM_GUIDE.md](TEAM_GUIDE.md) 참고.

## 구현

| 이름 | 내용 | 담당 |
|---|---|---|
| baseline | 커널 `rb_root` + spinlock/rwlock | 강성준 |
| skip v1 | skip list + 락 1개 (rbtree와 같은 조건 비교, v2 보험) | 이은찬 |
| maple tree | 커널 maple tree (읽기 RCU, 쓰기 락 1개). 커널의 최신 답과 비교 | 종주원 |
| skip v2 | lazy skip list, 노드별 락 (핵심) | 이우신 |
| skip v3 (선택) | RCU 기반 lock-free 탐색, 여유 있을 때만 | 종주원 |

## 폴더 구조

```
include/ds_ops.h      공통 인터페이스 (전원 합의)
harness/              측정 틀: kthread 워커, 작업 부하, ktime 측정 (강성준)
ds/                   구현: rbtree_lock.c, skiplist_v1.c, skiplist_v2.c, skiplist_rcu.c
tests/                정합성 검사 (이은찬)
scripts/              실험 실행 · 그래프 (종주원)
results/              측정 결과 CSV
```

## 빌드 방식

구현 하나 + harness를 묶어 모듈 하나로 만든다 (예: `bench_rb.ko`, `bench_sl1.ko`).
`Makefile`의 주석 처리된 줄을 구현이 생길 때마다 풀어서 추가한다.

```bash
make            # 모듈 빌드
sudo insmod bench_rb.ko   # 실행 (파라미터는 harness에서 정의)
dmesg | tail
sudo rmmod bench_rb
```

## 실험 변수

- 원소 수 N: 1만 / 10만 / 100만
- 스레드 수: 1 / 2 / 4 / 8 (VM의 vCPU 수까지)
- 작업 비율 (탐색·삽입·삭제): 100·0·0 / 90·5·5 / 50·25·25 / 0·50·50 — 삽입과 삭제를 같은 비율로 섞어 원소 수를 유지
- 범위 탐색 실험: 스캔 스레드 1 + 쓰기 스레드 1·2·4, 스캔 길이 100·1,000
- 조건마다 5번 반복 → 평균 ± 편차. 최종 측정은 한 사람의 VM에서 통일

## 협업 규칙

- `main`에 직접 push 금지. 각자 브랜치(`feat/...`)에서 작업 → PR
- **모든 PR은 강성준(PM)이 확인하고 approve·merge한다.** 강성준 본인의 PR은 이은찬이 확인한다 (GitHub은 자기 PR을 approve할 수 없다)
- `.github/CODEOWNERS`로 모든 PR의 리뷰 요청이 강성준에게 자동으로 간다. PR을 올릴 때는 템플릿의 체크리스트를 채운다
- 백업 짝: 강성준 ↔ 이은찬, 이우신 ↔ 종주원. 한 사람이 빠져도 짝이 그 파트를 설명할 수 있게 유지한다
- 구현마다 파일을 나눠서 충돌을 피한다
- 빌드 산출물(`*.o`, `*.ko` 등)은 커밋하지 않는다 (`.gitignore`)

## 일정

| 주차 | 강성준 (A) | 이은찬 (B) | 이우신 (C) | 종주원 (D) |
|---|---|---|---|---|
| 1주 (~11/24) | baseline + 측정 틀 | v1 구현 | v2 설계 | 스크립트 · 논문 조사 |
| 2주 (~12/1) | 통합 · 정합성 테스트 | C와 페어 | v2 구현 | 1차 측정 · 그래프 |
| 3주 (15주차 수업 전) | 최종 측정 | 분석 보조 | v2 안정화 · (선택) v3 | 슬라이드 · 리허설 |

## 참고

- W. Pugh, *Skip Lists: A Probabilistic Alternative to Balanced Trees*, CACM 1990
- M. Herlihy et al., *A Simple Optimistic Skiplist Algorithm* (lazy skip list), SIROCCO 2007
- Robert Love, *Linux Kernel Development*
