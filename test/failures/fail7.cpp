#include "ddw/impl.hpp"

struct A
{
  virtual ~A() {}
  virtual int zero() { return 0; }
};

struct B
{
  int zero() { return 0; }
};

int main()
{
  ddw::impl<A> a = std::make_shared<B>();
  return a->zero();
}
