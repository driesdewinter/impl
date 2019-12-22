#include "ddw/impl.hpp"

struct A
{
  virtual ~A() {}
  virtual int value() = 0;
};

struct EmplaceOnly : A
{
  EmplaceOnly(int v = 0) : v(v) {}
  EmplaceOnly(const EmplaceOnly& b) = delete;
  EmplaceOnly(EmplaceOnly&&) = delete;
  int value() { return v; }
  int v;
};


int main()
{
  ddw::impl<A> a;
  a.emplace_small<EmplaceOnly>(33);
  return a->value();
}
