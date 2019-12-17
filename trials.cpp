#include <functional>
#include <tuple>
#include <iostream>
#include <signal.h>
#include <dlfcn.h>
#include <vector>
#include <variant>
#include <memory>

#include "ddw/impl.hpp"

bool malloc_forbidden = false;

void *malloc(size_t size)
{
    if (malloc_forbidden) raise(SIGSEGV);
    static auto impl = reinterpret_cast<void*(*)(size_t)>(dlsym(RTLD_NEXT, "malloc"));
    return impl(size);
}

struct no_malloc_scope
{
    no_malloc_scope() { malloc_forbidden = true; }
    ~no_malloc_scope() { malloc_forbidden = false; }
};

struct ICallback
{
  virtual ~ICallback() {}
  virtual void Handle(int) = 0;
};

struct Registry
{
    std::vector<ddw::impl<ICallback>> data;
    void Add(ddw::impl<ICallback>&& cb) { data.emplace_back(std::move(cb)); }
    void Handle(int v) { for (auto& ent : data) ent->Handle(v); }
};

int main()
{
    struct C {
        int c = 3; int y = 5; int z = 6;
        std::array<int, 4> padding;
        std::string str = "gfdsgfsd";
        int x = 0;
        ~C() { c = 234567; x = 345677; y = 323456; z = 434567; }
    };

    std::vector<std::tuple<int, int>> v;

    Registry reg;

    struct MyCallback : ICallback
    {
        void Handle(int v) override { std::cerr << "Handle(" << v << ")\n";}
    };
    MyCallback cb;
    reg.Add(ddw::impl_by_reference(cb));
    reg.Add(ddw::impl_by_small_value(cb));
    reg.Add(ddw::impl_by_small_value(MyCallback()));
    reg.Handle(5);

    struct intf
    {
        virtual ~intf() {}
        virtual int triple(int) = 0;
        virtual int square(int) = 0;
        virtual int accum(int) = 0;
    };

    ddw::impl<intf, 100> instance;

    {
        no_malloc_scope bb;

        struct myimpl : intf
        {
            C c;
            myimpl() = default;
            myimpl(myimpl&&) = default;
            myimpl(const myimpl& other) : c(other.c) { std::cerr << "copy myimpl\n"; }
            int triple(int v) override { return v * 3; }
            int square(int v) override { return v * v; }
            int accum(int v) override { c.x += v; return c.x; }
        };
        const myimpl impl = myimpl();
        instance = myimpl();
    }

    std::variant<std::unique_ptr<intf>, typename std::aligned_storage<32, 8>::type> var;
    ddw::impl<intf> var2;
    std::cout << "vsize=" << sizeof(var) << "\n";
    std::cout << "v2size=" << sizeof(var2) << "\n";


    std::cout << "sizeof(instance)=" << sizeof(instance) << "\n";
    std::cout << "impl.triple(5)=" << instance->triple(5) << "\n";
    std::cout << "impl.square(5)=" << instance->square(5) << "\n";
    std::cout << "impl.accum(5)=" << instance->accum(5) << "\n";
    std::cout << "impl.accum(5)=" << instance->accum(5) << "\n";
    return 0;
}
