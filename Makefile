# 구현 하나 + harness = 모듈 하나
# 구현 파일이 생기면 해당 두 줄의 주석을 푼다.

obj-m += bench_dummy.o
bench_dummy-objs := harness/harness.o ds/dummy.o


# obj-m += bench_rb.o
# bench_rb-objs := harness/harness.o ds/rbtree_lock.o

# obj-m += bench_sl1.o
# bench_sl1-objs := harness/harness.o ds/skiplist_v1.o

# obj-m += bench_sl2.o
# bench_sl2-objs := harness/harness.o ds/skiplist_v2.o

# obj-m += bench_rcu.o
# bench_rcu-objs := harness/harness.o ds/skiplist_rcu.o

ccflags-y := -I$(src)/include

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

# Build validator tests with: make SL_VALIDATE_TEST=1
ifeq ($(SL_VALIDATE_TEST),1)
obj-m += test_sl_validate.o
test_sl_validate-objs := tests/skiplist_validate.o tests/skiplist_validate_test.o
endif
