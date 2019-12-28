#include "ddw/impl.hpp"

struct A
{
  virtual ~A() {}
  virtual int zero() = 0;
};

struct B : A
{
  int zero() { return 0; }
};

int main()
{
  ddw::impl<A, 32> a = B();
  ddw::impl<A, 36> b = std::move(a);
  return b->zero();
}
