#include "ddw/impl.hpp"

struct A
{
  virtual ~A() {}
  virtual int zero() { return 0; }
};

struct B : A
{
  std::aligned_storage_t<16, 16> v;

  int zero() { return 0; }
};

int main()
{
  ddw::impl<A> a = ddw::impl_by_small_value(B());
  return a->zero();
}
