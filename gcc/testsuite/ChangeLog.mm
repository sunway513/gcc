2011-11-02  Andrew MacLeod  <amacleod@redhat.com>

	* gcc.dg/atomic-noinline.c: New.  Make a variety of atomics calls.
	* gcc.dg/atomic-noinline-aux.c: New.  Provide some entry points.

2011-10-31  Richard Henderson  <rth@redhat.com>

	* lib/target-supports.exp (check_effective_target_cas_char): New.
	(check_effective_target_cas_int): New.
	* c-c++-common/gomp/atomic-10.c: Use cas_int; match __atomic builtin.
	* c-c++-common/gomp/atomic-3.c: Likewise.
	* c-c++-common/gomp/atomic-9.c: Likewise.

	* gcc.dg/gomp/atomic-1.c, gcc.dg/gomp/atomic-2.c,
	gcc.dg/gomp/atomic-3.c, gcc.dg/gomp/atomic-4.c, gcc.dg/gomp/atomic-7.c,
	gcc.dg/gomp/atomic-8.c, gcc.dg/gomp/atomic-9.c,
	gcc.dg/gomp/atomic-10.c, gcc.dg/gomp/atomic-12.c,
	gcc.dg/gomp/atomic-13.c, gcc.dg/gomp/atomic-14.c,
	gcc.dg/gomp/atomic-15.c: Move to c-c++-common/gomp/.
	* g++.dg/gomp/atomic-1.C, g++.dg/gomp/atomic-2.C,
	g++.dg/gomp/atomic-3.C, g++.dg/gomp/atomic-4.C, g++.dg/gomp/atomic-7.C,
	g++.dg/gomp/atomic-8.C, g++.dg/gomp/atomic-9.C,
	g++.dg/gomp/atomic-10.C, g++.dg/gomp/atomic-11.C,
	g++.dg/gomp/atomic-12.C, g++.dg/gomp/atomic-13.C,
	g++.dg/gomp/atomic-15.C: Remove.
	* gcc.dg/gomp/gomp.exp, g++.dg/gomp/gomp.exp: Run c-c++-common tests.

2011-10-25  Andrew MacLeod  <amacleod@redhat.com>

	* gcc.dg/atomic-generic-aux.c: New. Provide 4 functional external
	entrypoints for new generic library functions.
	* gcc.dg/atomic-generic.c: New. Test that generic functions produce
	the expected library calls.
	* gcc.dg/atomic-compare-exchange*.c: Replace __atomic_compare_exchange
	with __atomic_compare_exchange_n.  Add generic variations.
	* gcc.dg/atomic-exchange*.c: Replace __atomic_exchange with
	__atomic_exchange_n.  Add generic variations.
	* gcc.dg/atomic-invalid.c: Add _n to renamed functions.
	gcc.dg/atomic-load*.c: Replace __atomic_load with __atomic_load_n.  Add
	generic variations.
	* gcc.dg/atomic-param.c: Add _n to renamed functions.
	* gcc.dg/atomic-store*.c: Replace __atomic_store with __atomic_store_n.
	Add generic variations.
	* gcc.dg/simulate-thread/atomic*.c: Add _n to renamed functions.

2011-10-24  Aldy Hernandez  <aldyh@redhat.com>

	* lib/target-supports.exp (check_effective_target_sync_int_128):
	Handle both 32-bit and 64-bit triplets on x86.
	(check_effective_target_sync_long_long): Same.
	* gcc.dg/simulate-thread/atomic-load-int128.c: Handle i?86-*-*.
	* gcc.dg/simulate-thread/atomic-other-int128.c: Same.

2011-10-20  Andrew MacLeod  <amacleod@redhat.com>

	* gcc.dg/atomic-invalid.c: Add compare_exchange failures.
	* gcc.dg/atomic-compare-exchange-{1-5}.c: New functional tests.

2011-10-17  Andrew MacLeod  <amacleod@redhat.com>

	* sync-mem*.c: Rename all files to atomic-*, rename __sync_mem.
	* simulate-thread/sync*.c: Rename all files to atomic-*, rename 
	__sync_mem to __atomic..

2011-10-13  Andrew MacLeod  <amacleod@redhat.com>

	* testsuite/gcc.dg/sync-mem-invalid.c: Test for invalid param.
	* testsuite/gcc.dg/sync-mem-lockfree[-aux].c: New tests.

2011-10-10  Aldy Hernandez  <aldyh@redhat.com>

	Merge from mainline:

	2011-10-10  Aldy Hernandez  <aldyh@redhat.com>

	* lib/gcc-simulate-thread.exp: New.
	* gcc.dg/simulate-thread/guality.h: New.
	* gcc.dg/simulate-thread/simulate-thread.h: New.
	* gcc.dg/simulate-thread/simulate-thread.exp: New.
	* gcc.dg/simulate-thread/simulate-thread.gdb: New.
	* gcc.dg/simulate-thread/README: New.
	* g++.dg/simulate-thread/guality.h: New.
	* g++.dg/simulate-thread/simulate-thread.h: New.
	* g++.dg/simulate-thread/simulate-thread.exp: New.
	* g++.dg/simulate-thread/simulate-thread.gdb: New.
	* c-c++-common/cxxbitfields-2.c: Remove.
	* c-c++-common/cxxbitfields.c: Remove.
	* c-c++-common/cxxbitfields-4.c: Remove.
	* c-c++-common/cxxbitfields-5.c: Remove.
	* c-c++-common/simulate-thread/bitfields-1.c: New.
	* c-c++-common/simulate-thread/bitfields-2.c: New.
	* c-c++-common/simulate-thread/bitfields-3.c: New.
	* c-c++-common/simulate-thread/bitfields-4.c: New.

2011-10-04  Andrew MacLeod  <amacleod@redhat.com>

	* gcc.dg/memmodel/memmodel.h (__gdb_memmodel_fini): Rename from 
	memmodel_fini.
	(HOSTILE_THREAD_THRESHOLD, HOSTILE_THREAD_PAUSE): New.
	(__gdb_hostile_pause): New. Indicates a pause was triggered.
	(__gdb_wrapper_other_threads): New.  Wrapper for memmodel_other_threads.
	Avoid infinite loops.
	(__gdb_wrapper_final_verify): New. Wrapper for memmodel_final_verify.
	* gcc.dg/memmodel/memmodel.gdb: Use renamed __gdb_ routines instead.
	* gcc.dg/memmodel/speculative-store.c (memmodel_other_threads): Correct
	return type.
	* gcc.dg/memmodel/sync-load-int128.c: Don't xfail any more.
	* gcc.dg/memmodel/sync-other-int128.c: Don't xfail any more.

2011-09-16  Andrew MacLeod  <amacleod@redhat.com>

	* gcc.dg/sync-mem-invalid.c: Remove __sync_mem_flag_clear
	tests.
	* gcc.dg/sync-mem-flag.c: Remove.
	* gcc.dg/sync-mem-param.c: New testcase to check correct number of 
	parameters on a sample __sync_mem builtin.
	* gcc.dg/sync-mem-op-[1-5].c: Add NAND tests and a set of tests which 
	test the op builtins when the result isnt used.  Add a few constants 
	as parameters for better coverage.

2011-09-07  Aldy Hernandez  <aldyh@redhat.com>

	* gcc.dg/memmodel/memmodel.exp: Call torture-finish.
	* g++.dg/memmodel/memmodel.exp: Likewise.

2011-09-07  Aldy Hernandez  <aldyh@redhat.com>

	* gcc.dg/dg.exp: Exclude memmodel tests.

2011-08-18  Aldy Hernandez  <aldyh@redhat.com>

	* gcc.dg/memmodel/memmodel.exp: Torture with more options.
	* g++.dg/memmodel/memmodel.exp: Same.
	* gcc.dg/memmodel/*.c: Remove optimization flags.
	* g++.dg/memmodel/*.c: Remove optimization flags.

2011-08-18  Andrew MacLeod  <amacleod@redhat.com>

	* gcc.dg/memmodel/memmodel.gdb: Short-circuit tests once a failure 
	happens.
	* gcc.dg/memmodel/sync-other-{short,int,longlong,int128}.c: New. Test
	for proper atomic operation of all __sync_mem routines except load.
	* gcc.dg/memmodel/sync-load-{short,int,longlong,int128}.c: New. Test
	for proper atomic operation of __sync_mem_load.

2011-08-17  Aldy Hernandez  <aldyh@redhat.com>

	* lib/gcc-memmodel-gdb-test.exp (memmmodel-gdb-test): Handle xfails.

2011-08-17  Andrew MacLeod  <amacleod@redhat.com>

	* gcc.dg/memmodel/memmodel.gdb: Avoid pagination.
	* gcc.dg/sync-mem-invalid.c: Remove __sync_mem_compare_exchange test.

2011-08-13  Andrew MacLeod  <amacleod@redhat.com>

	* gcc.dg/sync-mem-op-[1-5].c: Combine all sync_mem_fetch_{op} 
	tests in one test file, and add new sync_mem_{op}_fetch tests.
	* gcc/dg/sync-mem-fetch-add-[1-5].c: Delete.
	* gcc/dg/sync-mem-fetch-sub-[1-5].c: Delete.
	* gcc/dg/sync-mem-fetch-and-[1-5].c: Delete.
	* gcc/dg/sync-mem-fetch-xor-[1-5].c: Delete.
	* gcc/dg/sync-mem-fetch-or-[1-5].c: Delete.
	
2011-07-27  Andrew MacLeod  <amacleod@redhat.com>

	* gcc.dg/sync-mem-{1-5}.c: Remove.
	* gcc.dg/sync-mem.h: Remove.
	* gcc.dg/sync-mem-exchange-{1-5}.c: New functional tests.
	* gcc.dg/sync-mem-fence.c: New functional tests.
	* gcc.dg/sync-mem-fetch-*-{1-5}.c: New functional tests.
	* gcc.dg/sync-mem-flag.c: New functional tests.
	* gcc.dg/sync-mem-invalid.c: Add new invalid combinations.
	* gcc.dg/sync-mem-load-{1-5}.c: New functional tests.
	* gcc.dg/sync-mem-store-{1-5}.c: New functional tests.

2011-06-23  Andrew MacLeod  <amacleod@redhat.com>

	* gcc.dg/sync-mem-invalid.c: Add invalid load tests.
	* gcc.dg/sync-mem.h: Add load executable tests.

2011-06-23  Andrew MacLeod  <amacleod@redhat.com>

	* lib/target-support.exp (check_effective_target_sync_int_128,
	check_effective_target_sync_long_long): Check whether the target
	supports 64 and 128 bit __sync builtins.
	* gcc.dg/sync-mem.h: New. Common code to check memory model __syncs.
	* gcc.dg/sync-mem-1.c: New. Check char size.
	* gcc.dg/sync-mem-2.c: New. Check short size.
	* gcc.dg/sync-mem-3.c: New. Check int size.
	* gcc.dg/sync-mem-4.c: New. Check long long.
	* gcc.dg/sync-mem-5.c: New. Check 128 bit.
	* gcc.dg/sync-mem-invalid.c: New. Check invalid memory modes.

2011-03-29  Aldy Hernandez  <aldyh@redhat.com>

	* gcc.dg/memmodel/subfields.c (set_a): Set noinline attribute.
	* g++.dg/memmodel/bitfields.C (set_a): Same.
	* g++.dg/memmodel/bitfields-2.C: New.

2011-03-29  Aldy Hernandez  <aldyh@redhat.com>

	* gcc.dg/memmodel/d2.c: Remove.
	* gcc.dg/memmodel/d3.c: Remove.
	* gcc.dg/memmodel/global-hoist.c: Remove.

2011-03-11  Aldy Hernandez  <aldyh@redhat.com>

	* lib/gcc-memmodel-gdb-test.exp: Return if no executable.
	* gcc.dg/memmodel/memmodel.h (memmodel_done): Add noinline
	attribute.
	* g++.dg/memmodel/bitfields.C: New.

2011-03-10  Aldy Hernandez  <aldyh@redhat.com>

	* lib/gcc-memmodel-gdb-test.exp: New.
	* gcc.dg/memmodel/speculative-store.c: New
	* gcc.dg/memmodel/subfields: New
	* gcc.dg/memmodel/guality.h: New
	* gcc.dg/memmodel/memmodel.h: New
	* gcc.dg/memmodel/global-hoist.c: New
	* gcc.dg/memmodel/d2.c: New
	* gcc.dg/memmodel/d3.c: New
	* gcc.dg/memmodel/memmodel.gdb: New.
	* gcc.dg/memmodel/memmodel.exp: New.
	* gcc.dg/memmodel/README: New.
	* g++.dg/memmodel/guality.h: New.
	* g++.dg/memmodel/memmodel.h: New.
	* g++.dg/memmodel/memmodel.gdb: New.
	* g++.dg/memmodel/memmodel.exp: New.
	* g++.dg/memmodel/atomics-1.C: New.

Local Variables:
mode: change-log
change-log-default-name: "ChangeLog.mm"
End:
