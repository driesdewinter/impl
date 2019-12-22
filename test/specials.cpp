#include "ddw/impl.hpp"
#include <gtest/gtest.h>

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

struct TrackedA : A
{
  TrackedA() : v(0) { Tracker::inst->default_constructed++; }
  TrackedA(int v) : v(v) { Tracker::inst->value_constructed++; }
  TrackedA(const EmplaceOnly& other) : v(other.v) { Tracker::inst->copy_constructed++; }
  TrackedA(EmplaceOnly&& other) :v(other.v) { Tracker::inst->move_constructed++; }
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
};

TEST(specials, copy_only)
{
  CopyOnly co(5);
  ddw::impl<A> a = co;
  ASSERT_EQ(5, a->value());
}

TEST(specials, emplace_only)
{
  ddw::impl<A> a = ddw::impl_emplace<EmplaceOnly>(77);
  ASSERT_EQ(77, a->value());
}

TEST(specials, emplaced_only)
{
  Tracker t;
  {
    ddw::impl<A> a = ddw::impl_emplace<TrackedA>(76);
    ASSERT_EQ(76, a->value());
  }
  ASSERT_EQ(0, t.copy_assigned);
  ASSERT_EQ(0, t.move_assigned);
  ASSERT_EQ(1, t.value_constructed);
  ASSERT_EQ(0, t.default_constructed);
  ASSERT_EQ(0, t.copy_constructed);
  ASSERT_EQ(0, t.move_constructed);
  ASSERT_EQ(1, t.destructed);
}
