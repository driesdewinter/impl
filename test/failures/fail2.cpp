#include "ddw/impl.hpp"

struct A
{
  virtual ~A() {}
  virtual int zero() = 0;
};

struct B : A
{
  B() = default;
  B(B&& b) = delete;
  int zero() { return 0; }
};

int main()
{
  ddw::impl<A> a = B();
  return a->zero();
}
