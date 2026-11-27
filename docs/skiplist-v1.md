# Skip List V1

## 범위와 의존성

전역 spinlock으로 보호하는 Skip List 기본 구현이다.
공유 노드 구조와 검사 함수는 PR #7에 의존한다.

이번 변경은 init, insert, search, remove, range, destroy와
V1 전용 테스트를 제공한다. 팀 공통 ds_ops는 변경하지 않는다.

## 구현 방식

- HEAD: key=0, height=20. 각 층의 끝은 NULL.
- 데이터 노드 높이: 1~20, 각 층에 1/2 확률로 승급.
- 노드 할당: GFP_KERNEL, 잠금을 잡기 전에 수행.
- 검색·삽입·삭제·범위 검색: 같은 전역 spinlock으로 보호.
- 삽입 중복 확인과 모든 층의 연결 변경은 잠금 안에서 수행.
- 검색은 잠금 안에서 존재 여부를 결정하고 bool만 반환.
- range는 양 끝을 포함한다. lo > hi이면 0을 반환.
- range는 전체 순회 동안 잠금을 유지하므로 정확한 개수를 반환한다.
  넓은 범위에서는 잠금 유지 시간이 길어질 수 있다.
- init/destroy는 워커와 동시에 실행하지 않는다.

## 반환값

- insert: 성공 0, 중복 -EEXIST, 할당 실패 -ENOMEM.
- remove: 성공 0, 없는 키 -ENOENT.
- search: 존재하면 true, 없으면 false.
- insert/remove에 예약 키 0 또는 U64_MAX를 넘기면 -EINVAL.
- 예약 키를 search하면 false.
- init을 이미 초기화된 상태에서 호출하면 -EBUSY.

## 삭제 노드 회수

삭제 노드는 모든 층에서 분리한 뒤 회수 대기 목록에 보관한다.
V1에서는 노드 포인터가 잠금 밖으로 노출되지 않으므로,
분리된 노드의 next[0]을 회수 대기 연결로 재사용한다.
이 방식은 V2에 그대로 적용할 수 없다.

모든 워커 종료 후 destroy가 살아 있는 노드와 회수 대기 노드를
모두 해제한다. 삭제 누적량에 따라 종료 전 메모리 사용량이 증가한다.
같은 키를 재삽입하면 새 노드를 할당한다.

## 빌드

```bash
make
make SL_VALIDATE_TEST=1 SL_V1_TEST=1 SL_V1_CONCURRENT_TEST=1
```

- bench_sl1.ko: 팀 하네스와 V1 구현.
- test_sl_validate.ko: 구조 검사 함수 자체의 테스트.
- test_sl1.ko: V1 연산과 구조 검사.
- test_sl1_concurrent.ko: 4개 워커의 동종 연산 경쟁 검사.

V1 테스트 파일은 내부 상태를 검사하기 위해 실제 구현 C 파일을
포함한다. 테스트 모듈에 ds/skiplist_v1.o를 별도로 중복 연결하지 않는다.
공통 구조 검사 구현은 별도 객체로 연결한다.

## 실행

아래 명령은 각 모듈이 로드되지 않은 상태에서 실행한다.
insmod가 실패하면 dmesg를 확인한다.

```bash
sudo insmod ./test_sl1.ko && sudo rmmod test_sl1
sudo insmod ./test_sl1_concurrent.ko && sudo rmmod test_sl1_concurrent
sudo dmesg | tail -n 40
```

기본 하네스 실행 예:

```bash
sudo insmod ./bench_sl1.ko nthreads=4 nkeys=1000 duration_ms=200 &&
sudo rmmod bench_sl1
```

현재 하네스는 사전 삽입 후 검색만 반복한다.
RESULT 출력은 삭제·range·검색 반환값의 정확성 검증을 대신하지 않는다.

## 확인한 결과

환경: Ubuntu VM, 5.15.0-vruntime+, aarch64, CPU 4개.

- V1 및 테스트 모듈 빌드 성공. 공유된 빌드 출력에 경고·오류 없음.
- 하네스: 키 1000개, 200ms, 1·4 스레드 실행과 제거 성공.
- 단일 스레드: sl1_test: passed=50 failed=0.
- 동시성: PASS phases=3 threads=4 keys=256.
- 동시 삽입·삭제·재삽입 각 단계:
  성공 256건, 예상 오류 768건, 예상 밖 오류 0건.
- 각 단계 구조 검사 0, 키 존재 여부 오류 0.
- 범위 개수: 삽입 256, 삭제 0, 재삽입 256.
- 구현 및 V1 테스트 C 파일 3개 checkpatch: 0 errors, 0 warnings.

## 검증 범위와 남은 작업

동시성 테스트는 단계별로 같은 종류의 연산을 경쟁시킨다.
삽입·삭제·검색이 동시에 섞이는 일반 혼합 스트레스는 별도 작업이다.

메모리 할당 실패 주입, 누수 검출기 검증, 커널 5.4 빌드·실행은
아직 수행하지 않았다. 짧은 하네스 실행 결과로 성능 우열을 판단하지 않는다.
