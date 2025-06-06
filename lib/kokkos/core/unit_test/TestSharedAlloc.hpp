//@HEADER
// ************************************************************************
//
//                        Kokkos v. 4.0
//       Copyright (2022) National Technology & Engineering
//               Solutions of Sandia, LLC (NTESS).
//
// Under the terms of Contract DE-NA0003525 with NTESS,
// the U.S. Government retains certain rights in this software.
//
// Part of Kokkos, under the Apache License v2.0 with LLVM Exceptions.
// See https://kokkos.org/LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//@HEADER

#include <gtest/gtest.h>

#include <sstream>
#include <iostream>

#include <Kokkos_Core.hpp>

/*--------------------------------------------------------------------------*/

namespace Test {

struct SharedAllocDestroy {
  volatile int* count;

  SharedAllocDestroy() = default;
  SharedAllocDestroy(int* arg) : count(arg) {}

  void destroy_shared_allocation() { Kokkos::atomic_inc(count); }
};

template <class MemorySpace, class ExecutionSpace>
void test_shared_alloc() {
  using Header     = const Kokkos::Impl::SharedAllocationHeader;
  using Tracker    = Kokkos::Impl::SharedAllocationTracker;
  using RecordBase = Kokkos::Impl::SharedAllocationRecord<void, void>;
  using RecordMemS = Kokkos::Impl::SharedAllocationRecord<MemorySpace, void>;
  using RecordFull =
      Kokkos::Impl::SharedAllocationRecord<MemorySpace, SharedAllocDestroy>;

  static_assert(sizeof(Tracker) == sizeof(int*),
                "SharedAllocationTracker has wrong size!");

  MemorySpace s;

  const size_t N    = 1200;
  const size_t size = 8;

  RecordMemS* rarray[N];
  Header* harray[N];

  RecordMemS** const r = rarray;
  Header** const h     = harray;

  Kokkos::RangePolicy<ExecutionSpace> range(0, N);

  {
    // Since always executed on host space, leave [=]
    Kokkos::parallel_for(range, [=](int i) {
      char name[64];
      snprintf(name, 64, "test_%.2d", i);

      r[i] = RecordMemS::allocate(s, name, size * (i + 1));
      h[i] = Header::get_header(r[i]->data());

      ASSERT_EQ(r[i]->use_count(), 0);

      for (int j = 0; j < (i / 10) + 1; ++j) RecordBase::increment(r[i]);

      ASSERT_EQ(r[i]->use_count(), (i / 10) + 1);
      ASSERT_EQ(r[i], RecordMemS::get_record(r[i]->data()));
    });

    Kokkos::fence();

#ifdef KOKKOS_ENABLE_DEBUG
    // Sanity check for the whole set of allocation records to which this record
    // belongs.
    RecordBase::is_sane(r[0]);
    // RecordMemS::print_records( std::cout, s, true );
#endif

    // This must be a plain for-loop since deallocation (which can be triggered
    // by RecordBase::decrement) fences all execution space instances. If this
    // is a parallel_for, the test can hang with the parallel_for blocking
    // waiting for itself to complete.
    for (size_t i = range.begin(); i < range.end(); ++i) {
      while (nullptr !=
             (r[i] = static_cast<RecordMemS*>(RecordBase::decrement(r[i])))) {
#ifdef KOKKOS_ENABLE_DEBUG
        if (r[i]->use_count() == 1) RecordBase::is_sane(r[i]);
#endif
      }
    }

    Kokkos::fence();
  }

  {
    int destroy_count = 0;
    SharedAllocDestroy counter(&destroy_count);

    Kokkos::parallel_for(range, [=](size_t i) {
      char name[64];
      snprintf(name, 64, "test_%.2d", int(i));

      RecordFull* rec = RecordFull::allocate(s, name, size * (i + 1));

      rec->m_destroy = counter;

      r[i] = rec;
      h[i] = Header::get_header(r[i]->data());

      ASSERT_EQ(r[i]->use_count(), 0);

      for (size_t j = 0; j < (i / 10) + 1; ++j) RecordBase::increment(r[i]);

      ASSERT_EQ(r[i]->use_count(), int((i / 10) + 1));
      ASSERT_EQ(r[i], RecordMemS::get_record(r[i]->data()));
    });

    Kokkos::fence();

#ifdef KOKKOS_ENABLE_DEBUG
    RecordBase::is_sane(r[0]);
#endif

    // This must be a plain for-loop since deallocation (which can be triggered
    // by RecordBase::decrement) fences all execution space instances. If this
    // is a parallel_for, the test can hang with the parallel_for blocking
    // waiting for itself to complete.
    for (size_t i = range.begin(); i < range.end(); ++i) {
      while (nullptr !=
             (r[i] = static_cast<RecordMemS*>(RecordBase::decrement(r[i])))) {
#ifdef KOKKOS_ENABLE_DEBUG
        if (r[i]->use_count() == 1) RecordBase::is_sane(r[i]);
#endif
      }
    }

    Kokkos::fence();

    ASSERT_EQ(destroy_count, int(N));
  }

  {
    int destroy_count = 0;

    {
      RecordFull* rec = RecordFull::allocate(s, "test", size);

      // ... Construction of the allocated { rec->data(), rec->size() }

      // Copy destruction function object into the allocation record.
      rec->m_destroy = SharedAllocDestroy(&destroy_count);

      ASSERT_EQ(rec->use_count(), 0);

      // Start tracking, increments the use count from 0 to 1.
      Tracker track;

      track.assign_allocated_record_to_uninitialized(rec);

      ASSERT_EQ(rec->use_count(), 1);
      ASSERT_EQ(track.use_count(), 1);

      // Verify construction / destruction increment.
      for (size_t i = 0; i < N; ++i) {
        ASSERT_EQ(rec->use_count(), 1);

        {
          Tracker local_tracker;
          local_tracker.assign_allocated_record_to_uninitialized(rec);
          ASSERT_EQ(rec->use_count(), 2);
          ASSERT_EQ(local_tracker.use_count(), 2);
        }

        ASSERT_EQ(rec->use_count(), 1);
        ASSERT_EQ(track.use_count(), 1);
      }

      Kokkos::parallel_for(range, [=](size_t) {
        Tracker local_tracker;
        local_tracker.assign_allocated_record_to_uninitialized(rec);
        ASSERT_GT(rec->use_count(), 1);
      });

      Kokkos::fence();

      ASSERT_EQ(rec->use_count(), 1);
      ASSERT_EQ(track.use_count(), 1);

      // Destruction of 'track' object deallocates the 'rec' and invokes the
      // destroy function object.
    }

    ASSERT_EQ(destroy_count, 1);
  }
}

TEST(TEST_CATEGORY, impl_shared_alloc) {
#ifdef TEST_CATEGORY_NUMBER
#if (TEST_CATEGORY_NUMBER < 4)  // serial threads openmp hpx
  test_shared_alloc<Kokkos::HostSpace, TEST_EXECSPACE>();
#elif (TEST_CATEGORY_NUMBER == 4)  // openmptarget
  test_shared_alloc<Kokkos::Experimental::OpenMPTargetSpace,
                    Kokkos::DefaultHostExecutionSpace>();
#elif (TEST_CATEGORY_NUMBER == 5)  // cuda
  test_shared_alloc<Kokkos::CudaSpace, Kokkos::DefaultHostExecutionSpace>();
#elif (TEST_CATEGORY_NUMBER == 6)  // hip
  test_shared_alloc<Kokkos::HIPSpace, Kokkos::DefaultHostExecutionSpace>();
#elif (TEST_CATEGORY_NUMBER == 7)  // sycl
  test_shared_alloc<Kokkos::SYCLDeviceUSMSpace,
                    Kokkos::DefaultHostExecutionSpace>();
#elif (TEST_CATEGORY_NUMBER == 8)  // openacc
  test_shared_alloc<Kokkos::Experimental::OpenACCSpace,
                    Kokkos::DefaultHostExecutionSpace>();
#endif
#else
  test_shared_alloc<TEST_EXECSPACE, Kokkos::DefaultHostExecutionSpace>();
#endif
}

}  // namespace Test
