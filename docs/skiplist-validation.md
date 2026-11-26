# Skip list node layout and structural validation

## Scope

This change provides a proposed shared node layout, a structural
validator, and a standalone kernel test module.

It does not implement skip list v1 operations, range-result tests,
or concurrent stress tests.

## Node layout

- key: unsigned 64-bit key.
- height: number of levels, from 1 through SL_MAX_LEVEL (20).
- next[]: flexible array containing height pointers.
- A node of height 3 participates in levels 0, 1, and 2.
- The head sentinel has key 0 and height SL_MAX_LEVEL.
- Every level ends with NULL. There is no tail sentinel.
- Data keys exclude 0 and U64_MAX.

The allocation must include storage for the pointer array.
sizeof(struct sl_node) alone is insufficient.

This layout is a proposal for coordination with the v2 owner.
V2 synchronization and retired-node bookkeeping are not included.
Do not cast an incompatible v2 node layout to struct sl_node.

## Validator

Include skiplist_validate.h and link tests/skiplist_validate.o
into the calling module.

Call:

    ret = sl_validate(head, expected_count);

The function returns 0 on success and -EINVAL on a detected violation.

Checks:

- Head key and height.
- Data-node height and reserved keys.
- Strictly increasing keys on level 0.
- Level-0 count equals expected_count.
- Each upper level contains exactly the same node objects from the
  preceding level whose heights include that upper level.

Upper-level ordering follows from the validated lower-level ordering.

## Preconditions and limits

- Stop and join all workers before validation.
- No concurrent readers, writers, or memory reclamation.
- Call from sleepable process context without holding a spinlock;
  the validator periodically calls cond_resched().
- Reachable data nodes must be fully linked and logically live.
- All inspected pointers must refer to valid allocations with
  enough storage for their declared heights.
- Arbitrary invalid pointers and freed memory cannot be safely checked
  by this function.
- expected_count excludes the head and detached retired nodes.
- The validator neither modifies the list nor acquires locks.

For a populated test:

    expected_count = initial_count + successful_inserts - successful_removes

Count successful operations, not attempted operations.
Range-result correctness requires separate functional tests.

## Build and run the validator tests

    make SL_VALIDATE_TEST=1
    sudo insmod ./test_sl_validate.ko
    sudo rmmod test_sl_validate
    sudo dmesg | grep 'sl_validate_test:'

Expected summary:

    sl_validate_test: passed=18 failed=0

The test allocates a small fixture, checks valid and deliberately
invalid structures, restores changed fields, and frees all fixture
allocations before module initialization returns.

If any check fails, initialization returns -EINVAL and the module
is not loaded. Inspect dmesg for the failed case.

## Validation recorded for this change

- Kernel: 5.15.0-vruntime+, aarch64.
- Test module built without reported compiler warnings.
- Module loaded, all 18 checks passed, and module unloaded.
- checkpatch.pl: 0 errors and 0 warnings for all four C/header files.
- Kernel 5.4 build and execution: not yet tested.
- These results validate the checker, not a completed skip list.
