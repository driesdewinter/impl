#include "ddw/impl.hpp"
#include <gtest/gtest.h>
#include <dlfcn.h>

namespace
{

struct A
{
  virtual ~A() {}
  virtual int value() = 0;
};

struct CopyOnly : A
{
  CopyOnly(int v = 0) : v(v) {}
  CopyOnly(const CopyOnly&) = default;
  CopyOnly(CopyOnly&&) = delete;
  int value() { return v; }
  int v;
};

struct EmplaceOnly : A
{
  EmplaceOnly(int v = 0) : v(v) {}
  EmplaceOnly(const EmplaceOnly&) = delete;
  EmplaceOnly(EmplaceOnly&&) = delete;
  int value() { return v; }
  int v;
};

struct Tracker
{
  static Tracker* inst;
  Tracker() { inst = this; }
  ~Tracker() { inst = nullptr; }

  int default_constructed = 0;
  int value_constructed = 0;
  int move_constructed = 0;
  int copy_constructed = 0;
  int move_assigned = 0;
  int copy_assigned = 0;
  int destructed = 0;
};
Tracker* Tracker::inst = nullptr;

struct SmallCapture
{
};

struct LargeCapture
{
  int arr[33];
};

template<class ExtraCapture>
struct TrackedA : A
{
  TrackedA() : v(0) { Tracker::inst->default_constructed++; }
  TrackedA(int v) : v(v) { Tracker::inst->value_constructed++; }
  TrackedA(const TrackedA& other) : v(other.v) { Tracker::inst->copy_constructed++; }
  TrackedA(TrackedA&& other) :v(other.v) { Tracker::inst->move_constructed++; }
  TrackedA& operator=(const TrackedA& other)
  {
    Tracker::inst->copy_assigned++;
    v = other.v;
    return *this;
  }
  TrackedA& operator=(TrackedA&& other)
  {
    Tracker::inst->move_assigned++;
    v = other.v;
    return *this;
  }
  ~TrackedA() { Tracker::inst->destructed++; }
  int value() { return v; }

  int v;
  ExtraCapture extra;
};

using SmallTrackedA = TrackedA<SmallCapture>;
using LargeTrackedA = TrackedA<LargeCapture>;

struct MallocTracker
{
  static MallocTracker* inst;
  MallocTracker() { inst = this; }
  ~MallocTracker() { inst = nullptr; }

  int malloced = 0;
  int freed = 0;
};
MallocTracker* MallocTracker::inst = nullptr;

}

void *malloc(size_t size)
{
  static auto impl = reinterpret_cast<void*(*)(size_t)>(dlsym(RTLD_NEXT, "malloc"));
  if (MallocTracker::inst) MallocTracker::inst->malloced++;
  return impl(size);
}

void free(void *p)
{
  static auto impl = reinterpret_cast<void(*)(void*)>(dlsym(RTLD_NEXT, "free"));
  if (MallocTracker::inst) MallocTracker::inst->freed++;
  impl(p);
}

TEST(specials, copy_only)
{
  CopyOnly co(5);
  ddw::impl<A> a = co;
  ASSERT_EQ(5, a->value());
}

TEST(specials, emplace_only)
{
  MallocTracker mt;
  {
    ddw::impl<A> a = ddw::impl_emplace<EmplaceOnly>(77);
    ddw::impl<A> a2 = std::move(a);
    ASSERT_EQ(77, a2->value());
  }
  ASSERT_EQ(1, mt.malloced);
  ASSERT_EQ(1, mt.freed);
}

TEST(specials, emplaced_only)
{
  MallocTracker mt;
  Tracker t;
  {
    ddw::impl<A> a = ddw::impl_emplace<SmallTrackedA>(76);
    ASSERT_EQ(76, a->value());
  }
  ASSERT_EQ(0, t.copy_assigned);
  ASSERT_EQ(0, t.move_assigned);
  ASSERT_EQ(1, t.value_constructed);
  ASSERT_EQ(0, t.default_constructed);
  ASSERT_EQ(0, t.copy_constructed);
  ASSERT_EQ(0, t.move_constructed);
  ASSERT_EQ(1, t.destructed);
  ASSERT_EQ(0, mt.malloced);
  ASSERT_EQ(0, mt.freed);
}

TEST(specials, large_value)
{
  MallocTracker mt;
  Tracker t;
  {
    ddw::impl<A> a = ddw::impl_emplace<LargeTrackedA>(75);
    ASSERT_EQ(75, a->value());
  }
  ASSERT_EQ(0, t.copy_assigned);
  ASSERT_EQ(0, t.move_assigned);
  ASSERT_EQ(1, t.value_constructed);
  ASSERT_EQ(0, t.default_constructed);
  ASSERT_EQ(0, t.copy_constructed);
  ASSERT_EQ(0, t.move_constructed);
  ASSERT_EQ(1, t.destructed);
  ASSERT_EQ(1, mt.malloced);
  ASSERT_EQ(1, mt.freed);
}

TEST(specials, fit_value)
{
  using AImpl = ddw::impl<A, sizeof(LargeTrackedA)>;
  MallocTracker mt;
  Tracker t;
  {
    AImpl a = ddw::impl_emplace<LargeTrackedA>(75);
    ASSERT_EQ(75, a->value());
  }
  ASSERT_EQ(0, t.copy_assigned);
  ASSERT_EQ(0, t.move_assigned);
  ASSERT_EQ(1, t.value_constructed);
  ASSERT_EQ(0, t.default_constructed);
  ASSERT_EQ(0, t.copy_constructed);
  ASSERT_EQ(0, t.move_constructed);
  ASSERT_EQ(1, t.destructed);
  ASSERT_EQ(0, mt.malloced);
  ASSERT_EQ(0, mt.freed);
}

TEST(specials, one_more)
{
  struct LargerCapture { LargeCapture base; char extra = 'a'; };
  using LargerTrackedA = TrackedA<LargerCapture>;
  using AImpl = ddw::impl<A, sizeof(LargeTrackedA)>;
  MallocTracker mt;
  Tracker t;
  {
    AImpl a = ddw::impl_emplace<LargerTrackedA>(74);
    ASSERT_EQ(74, a->value());
  }
  ASSERT_EQ(0, t.copy_assigned);
  ASSERT_EQ(0, t.move_assigned);
  ASSERT_EQ(1, t.value_constructed);
  ASSERT_EQ(0, t.default_constructed);
  ASSERT_EQ(0, t.copy_constructed);
  ASSERT_EQ(0, t.move_constructed);
  ASSERT_EQ(1, t.destructed);
  ASSERT_EQ(1, mt.malloced);
  ASSERT_EQ(1, mt.freed);
}

TEST(specials, move_small_value)
{
  MallocTracker mt;
  Tracker t;
  {
    ddw::impl<A> a = ddw::impl_emplace<SmallTrackedA>(3);
    ddw::impl<A> a2 = std::move(a);
    ASSERT_EQ(3, a2->value());
  }
  ASSERT_EQ(0, t.copy_assigned);
  ASSERT_EQ(0, t.move_assigned);
  ASSERT_EQ(1, t.value_constructed);
  ASSERT_EQ(0, t.default_constructed);
  ASSERT_EQ(0, t.copy_constructed);
  ASSERT_EQ(1, t.move_constructed);
  ASSERT_EQ(2, t.destructed);
  ASSERT_EQ(0, mt.malloced);
  ASSERT_EQ(0, mt.freed);
}

TEST(specials, user_unique_ptr)
{
  MallocTracker mt;
  Tracker t;
  {
    ddw::impl<A> a = std::make_unique<SmallTrackedA>(33);
    ASSERT_EQ(33, a->value());
  }
  ASSERT_EQ(0, t.copy_assigned);
  ASSERT_EQ(0, t.move_assigned);
  ASSERT_EQ(1, t.value_constructed);
  ASSERT_EQ(0, t.default_constructed);
  ASSERT_EQ(0, t.copy_constructed);
  ASSERT_EQ(0, t.move_constructed);
  ASSERT_EQ(1, t.destructed);
  ASSERT_EQ(1, mt.malloced);
  ASSERT_EQ(1, mt.freed);
}

TEST(specials, user_shared_ptr)
{
  MallocTracker mt;
  Tracker t;
  {
    ddw::impl<A> a = std::make_shared<SmallTrackedA>(33);
    ASSERT_EQ(33, a->value());
  }
  ASSERT_EQ(0, t.copy_assigned);
  ASSERT_EQ(0, t.move_assigned);
  ASSERT_EQ(1, t.value_constructed);
  ASSERT_EQ(0, t.default_constructed);
  ASSERT_EQ(0, t.copy_constructed);
  ASSERT_EQ(0, t.move_constructed);
  ASSERT_EQ(1, t.destructed);
  ASSERT_EQ(1, mt.malloced);
  ASSERT_EQ(1, mt.freed);
}

TEST(specials, default_null)
{
  MallocTracker mt;
  Tracker t;
  {
    ddw::impl<A> a;
    ASSERT_FALSE(a);
    a = SmallTrackedA(100);
    ASSERT_TRUE(a);
    ASSERT_EQ(100, a->value());
  }
  ASSERT_EQ(0, t.copy_assigned);
  ASSERT_EQ(0, t.move_assigned);
  ASSERT_EQ(1, t.value_constructed);
  ASSERT_EQ(0, t.default_constructed);
  ASSERT_EQ(0, t.copy_constructed);
  ASSERT_EQ(1, t.move_constructed);
  ASSERT_EQ(2, t.destructed);
  ASSERT_EQ(0, mt.malloced);
  ASSERT_EQ(0, mt.freed);
}

TEST(specials, convert_small_impl)
{
  MallocTracker mt;
  Tracker t;
  {
    ddw::impl<SmallTrackedA> a1 = SmallTrackedA(22);
    ddw::impl<A> a2 = std::move(a1);
    ASSERT_EQ(22, a2->value());
  }
  ASSERT_EQ(0, t.copy_assigned);
  ASSERT_EQ(0, t.move_assigned);
  ASSERT_EQ(1, t.value_constructed);
  ASSERT_EQ(0, t.default_constructed);
  ASSERT_EQ(0, t.copy_constructed);
  ASSERT_EQ(2, t.move_constructed);
  ASSERT_EQ(3, t.destructed);
  ASSERT_EQ(0, mt.malloced);
  ASSERT_EQ(0, mt.freed);
}

TEST(specials, convert_large_impl)
{
  MallocTracker mt;
  Tracker t;
  {
    ddw::impl<LargeTrackedA> a1 = LargeTrackedA(22);
    ddw::impl<A> a2 = std::move(a1);
    ASSERT_EQ(22, a2->value());
  }
  ASSERT_EQ(0, t.copy_assigned);
  ASSERT_EQ(0, t.move_assigned);
  ASSERT_EQ(1, t.value_constructed);
  ASSERT_EQ(0, t.default_constructed);
  ASSERT_EQ(0, t.copy_constructed);
  ASSERT_EQ(1, t.move_constructed);
  ASSERT_EQ(2, t.destructed);
  ASSERT_EQ(1, mt.malloced);
  ASSERT_EQ(1, mt.freed);
}
