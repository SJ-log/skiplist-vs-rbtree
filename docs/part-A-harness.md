# A 파트 인수인계 — 측정 틀 + rbtree + PM (강성준)

> 새 세션에서 루트 `CLAUDE.md`와 이 문서만 읽고 바로 시작할 수 있게 정리했다.
> 전체 그림은 `docs/index.html`(팀 공유 페이지)에 있다.

## 한 줄 요약

모든 구현을 같은 조건에서 공정하게 재는 **측정 틀(harness)**과 비교 기준인 **rbtree(spinlock·rwlock)**를 만든다.
다른 사람의 결과가 전부 이 틀을 거친다. 팀 병목 중 ② 측정 틀과 ⑤ PR 확인·통합 쏠림이 A에게 걸려 있다.

## 지금 상태 (2026-11-24 기준)

- `main`에 들어간 것: PR #1 팀 페이지와 규칙, PR #2 커널 5.4에 맞춘 계획 수정, PR #3 1단계(`harness/harness.c`, `ds/dummy.c` → `bench_dummy.ko`), PR #4 문서를 측정 틀에 맞춤
- **1단계 완료.** 더미 측정값(vCPU 4): 1스레드 2.75억, 2스레드 5.33억(1.93배), 4스레드 10.44억 ops/s(3.79배). 이 곡선이 이 VM에서 가능한 최대 확장이므로 다른 구현의 확장성은 이 값과 비교한다. vCPU를 8개로 늘리면 1·2·4·8로 다시 잰다
- 워커 구조체는 128바이트로 정렬한다 (`BENCH_CACHE_ALIGN`). Apple Silicon의 캐시 라인이 128바이트라서 `____cacheline_aligned_in_smp`(arm64에서 64바이트)로는 false sharing이 났다 (더미 4스레드 3.16배 → 3.79배)
- 경과 시간은 3초보다 0.05~0.2초 길게 나온다 (타이머가 올림해서 깨우는 것으로 보인다). 처리량은 반드시 `elapsed_ns`로 나눈다
- **첫 회의를 했다 (2026-11-24).** ds_ops 규칙과 RESULT 형식은 제안대로, xarray 확정, 스레드 1·2·4·8, 최종 측정은 내 VM. 팀원 VM은 커널 5.15.0이고 코어는 8개까지 늘릴 수 있다
- 아직 안 정한 것: 범위 탐색의 쓰기 스레드 수(문서에는 1·2·4로 적었다), 가설 성립 기준(v2가 xarray를 앞서야 하는지, v2만 계속 오르면 되는지), 주 1회 점검 시간
- 아직 안 한 PM 일: 협업자 초대, `main` 보호 규칙
- 다음 할 일: `feat/ds-ops` PR을 VM에서 빌드해 보고 merge → vCPU를 8개로 늘려 더미 기준선을 다시 잰다 → 2단계(작업 비율)
- 팀원 VM(커널 5.15)에서 `bench_dummy`가 빌드되고 도는지 확인을 부탁해 둔다
- `tests/`, `scripts/`는 아직 없다

## 내 VM 정보 (2026-11-23 확인)

다음 세션이 헤더 버전에 맞춰 코드를 쓰도록 VM에서 확인해 적어 둔다.

| 항목 | 명령 | 값 |
|---|---|---|
| 커널 버전 | `uname -r` | 5.4.214SJKang+ |
| 아키텍처 | `uname -m` | aarch64 |
| vCPU 수 | `nproc` | 4 (최종 측정 전에 8개로 늘린다) |
| 배포판 | `lsb_release -d` | Ubuntu 20.04.5 LTS |

이 환경에서 주의할 점:

- 커널 5.4 → 워커 종료는 `complete_and_exit`를 쓴다 (`kthread_complete_and_exit`는 5.17부터)
- maple tree는 6.1부터 있어서 이 커널에서는 빌드할 수 없다 → D 파트는 xarray로 확정했다
- 스레드 수는 1·2·4·8로 확정했다. vCPU를 8개로 늘려야 `nthreads=8`을 돌릴 수 있다. 호스트의 코어가 8개 이상인지, 더미가 8스레드에서도 제대로 오르는지 먼저 확인한다
- 팀원 VM은 커널 5.15.0이다. 코드는 5.4와 5.15에서 모두 빌드되어야 한다
- `/lib/modules/$(uname -r)/build`가 있어서 모듈 빌드는 바로 된다 (확인함)

---

## 0. PM으로 먼저 할 일

- [x] **첫 회의** (2026-11-24) — 아래를 확정했다
  1. `insert` 반환값 (0 / `-EEXIST` / `-ENOMEM`)
  2. `remove` 반환값 (0 / `-ENOENT`)
  3. `range(lo, hi)` 의미 ([lo, hi] 안의 키 개수, 동시 수정 중에는 근사치 허용)
  4. 키 범위 (사용자 키는 `[1, 2 × nkeys]`. 0과 `U64_MAX`는 센티넬용으로 비워 둔다)
  5. `init` · `destroy`는 한 스레드에서만 호출
  6. RESULT 형식은 아래 그대로. 최종 측정은 내 VM
  7. maple tree 대신 xarray. 팀원 VM의 커널은 5.15.0
  8. 스레드 수 1·2·4·8. 범위 탐색의 쓰기 스레드는 아직 안 정했다 (문서에는 1·2·4)
- [ ] 회의 결과를 `include/ds_ops.h`에 반영하는 PR (`range` 추가, 반환값 주석) — `feat/ds-ops` 브랜치로 올렸다
- [ ] GitHub: 협업자 초대, `main` 보호 규칙 (Settings → Branches: PR 필수, 승인 1명)
  - `.github/CODEOWNERS`가 모든 PR의 리뷰 요청을 나에게 보낸다
  - "Do not allow bypassing the above settings"는 켜지 않는다. 꺼 두면 저장소 관리자인 내 PR은 승인 없이 merge할 수 있다
  - "Require review from Code Owners"를 켜면 팀원의 PR에 내 승인이 꼭 필요해진다 (선택)
- [ ] **모든 PR 확인과 merge**: 내 코드 작업보다 리뷰 요청을 먼저 처리한다. 확인 기준은 PR 템플릿 체크리스트
- [ ] 주 1회 15분 점검: 각자 "끝낸 것 / 막힌 것 / 다음 할 것"만

## 1~6. 구현 단계

순서대로 진행하고 단계마다 `feat/harness` 브랜치에서 PR을 올린다. 내 PR은 리뷰 없이 직접 merge한다.

### 1단계 — 더미 구현 + 최소 측정 틀 (병목 ② 해소) — 완료 (PR #3)

- 파일: `harness/harness.c`, `ds/dummy.c`(아무 일도 안 하는 `ds_ops`), Makefile에 `bench_dummy`
- 파라미터: `nthreads`, `nkeys`, `duration_ms`
- 흐름: `module_init` → 파라미터 검증 → `bench_ds.init()` → 미리 채우기 → 워커 생성 → 동시 출발 → `duration_ms` 뒤 정지 → 전원 종료 대기 → RESULT 한 줄 출력 → `module_init`이 0 반환(모듈은 남음) → `module_exit`에서 `bench_ds.destroy()`
- 끝나면 **종주원(D)에게 알린다** — D가 스크립트를 이 더미로 연결해 본다
- 완료: `sudo insmod bench_dummy.ko nthreads=4` 후 `dmesg`에 RESULT가 한 줄 찍힌다

### 2단계 — 작업 비율

- 파라미터 `search_pct`, `insert_pct` (삭제 = 100 − 둘의 합). 합이 100을 넘으면 `-EINVAL`
- 키: 스레드별 `struct rnd_state` + `prandom_u32_state()`, 키 = `1 + r % (2 × nkeys)`
- 미리 채우기: 무작위 키로 `nkeys`개가 실제로 들어갈 때까지 넣는다 (`-EEXIST`면 다시)
- 삽입과 삭제 비율이 같으므로 원소 수는 대략 `nkeys`로 유지된다

### 3단계 — rbtree (`ds/rbtree_lock.c`, `ds/rbtree_rw.c`)

- spinlock 버전 → `bench_rb.ko`
- rwlock 버전 → `bench_rbw.ko`. 같은 소스를 두 번 쓰려면 `ds/rbtree_rw.c`에 `#define RB_RWLOCK` 후 `#include "rbtree_lock.c"` (오브젝트 이름이 달라야 해서)
- 노드 할당은 락 밖에서. 중복이면 락을 푼 뒤 `kfree`
- 삭제는 락 안에서 `rb_erase`, `kfree`는 락을 푼 뒤. 전역 락이라 락 밖에서 이 노드를 보는 스레드가 없어서 바로 해제해도 된다
- `range`: lower bound를 찾고 `rb_next`로 `hi`까지 센다 (spin은 락, rw는 읽기 락 안에서)
- `destroy`: `rbtree_postorder_for_each_entry_safe`로 전부 `kfree`

### 4단계 — 스레드 1~8, CPU 고정, false sharing

- `nthreads > num_online_cpus()`면 거부한다 (측정 의미가 없어진다)
- 워커 i는 CPU `i % num_online_cpus()`에 고정
- 워커 구조체는 128바이트로 정렬한다 (`BENCH_CACHE_ALIGN`, 1단계에서 반영함). `____cacheline_aligned_in_smp`는 64바이트라 부족하다
- 1024연산마다 락 밖에서 `cond_resched()`

### 5단계 — 범위 탐색 스레드

- 파라미터 `scan_threads`(0 · 1), `scan_len`(100 · 1,000)
- 스캔 스레드는 무작위 `lo`에서 `range(lo, lo + scan_len - 1)`을 반복하고, 스캔 횟수를 따로 센다
- 스캔 스레드는 `nthreads`(워커 수)에 포함하지 않는다
- `nthreads + scan_threads > num_online_cpus()`면 거부한다 (vCPU 8개면 쓰기 4 + 스캔 1은 통과한다)

### 6단계 — 통합

- B·C·D 구현이 올라오면 Makefile에 쌍을 추가하고, B의 검사 함수로 모든 구현을 검증
- D와 함께 rbtree·v1로 1차 측정 → 같은 조건 5번의 편차를 확인

## RESULT 형식 (2026-11-24 확정)

```
RESULT,ds,threads,nkeys,search,insert,remove,scan_len,ops,scan_ops,elapsed_ns
```

- 측정 틀이 실제로 출력하는 형식이고, 팀 페이지에도 같은 형식으로 적었다
- 처리량 = `ops / elapsed_ns × 1e9`, 평균 지연 ≈ `elapsed_ns × threads / ops`

## 설계 메모

### 측정 틀 뼈대

```c
struct worker {
	struct task_struct *task;
	struct rnd_state rnd;
	struct completion done;
	u64 ops;
	int id;
} __aligned(BENCH_CACHE_ALIGN);   /* 128바이트 */

static DECLARE_COMPLETION(start);   /* 출발 신호: complete_all() */
static bool stop;                   /* READ_ONCE / WRITE_ONCE */
```

- 워커: `wait_for_completion(&start)` → `while (!READ_ONCE(stop))` 루프 → `complete_and_exit(&w->done, 0)` (커널 5.4 기준. 5.17부터는 `kthread_complete_and_exit`)
- 메인: `kthread_create` → `kthread_bind` → `wake_up_process` (bind는 반드시 wake 전에) → 전원 준비 확인 → `t0 = ktime_get_ns()` → `complete_all(&start)` → `msleep(duration_ms)` → `WRITE_ONCE(stop, true)` → 각 워커의 `done` 대기 → `t1`
- 출발 신호를 busy-wait로 기다리면 메인 스레드가 CPU를 못 얻을 수 있다. completion으로 잠들어 기다린다
- 측정 루프에 `cond_resched()`가 없으면 vCPU 수 = 스레드 수일 때 `stop`을 거는 메인이 늦게 깬다
- `module_init` 안에서 측정을 끝내므로 `insmod`는 미리 채우기 + 측정 시간만큼 기다린다. D의 스크립트는 그 뒤 `dmesg`를 읽고 `rmmod`한다

### rbtree 삽입 뼈대

```c
struct rb_key {
	struct rb_node node;
	u64 key;
};

n = kmalloc(sizeof(*n), GFP_KERNEL);   /* 락 밖 */
spin_lock(&lock);
/* link를 따라 내려가며 위치 찾기, 같은 키면 unlock → kfree → -EEXIST */
rb_link_node(&n->node, parent, link);
rb_insert_color(&n->node, &root);
spin_unlock(&lock);
```

- 공정성: 모든 구현이 삽입마다 `kmalloc`을 하므로 할당 비용 조건이 같다. 나중에 바꾼다면 전부 `kmem_cache`로 같이 바꾼다

## 완료 기준

- `bench_rb.ko`, `bench_rbw.ko`가 nthreads=1·2·4·8에서 RESULT를 출력한다
- 같은 조건 5번의 처리량 편차가 10% 안팎이다
- `rmmod` 후 누수가 없다 (성공한 insert − remove 개수와 destroy에서 해제한 개수가 일치)
- 더미 구현으로 D의 스크립트가 처음부터 끝까지 돈다

## 함정 체크리스트

- [ ] 스핀락 안에서 `kmalloc(GFP_KERNEL)` 금지
- [ ] 반복문 안에서 `pr_info` 금지
- [ ] `kthread_bind`는 `wake_up_process` 전에
- [ ] 실험 전 VM 스냅샷, 다른 터미널에 `dmesg -w`, `insmod` 전에 `sync`
- [ ] 작은 `nkeys`부터 단계적으로 늘린다

## 발표 파트 (1분)

실험 환경과 방법: VM 사양(vCPU, 메모리, 커널 버전), 측정 방법(동시 출발, 3초, 5회 반복), 공정성 장치(CPU 고정, 같은 할당 방식, 같은 키 분포).

---

## 새 세션에서 붙여 넣을 말

```
skiplist-vs-rbtree 저장소의 CLAUDE.md와 docs/part-A-harness.md를 읽고,
최신 main에서 feat/harness 브랜치를 만들어 "지금 상태"의 다음 할 일부터 이어서 해 줘.
빌드와 실행은 내 VM에서 하고 결과를 붙여 줄게. VM 정보는 문서의 표에 적어 뒀어.
```
