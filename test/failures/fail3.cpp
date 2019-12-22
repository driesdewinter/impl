#include "ddw/impl.hpp"

struct A
{
  virtual ~A() {}
  virtual int zero() = 0;
};

struct B : A
{
  int arr[33];

  int zero() { return 0; }
};

int main()
{
  ddw::impl<A> a = ddw::impl_by_small_value(B());
  return a->zero();
}
