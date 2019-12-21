# ddw::impl<T> - Traditional OO meets value semantics
## The problem
Imagine that we have some kind of printing service for different types of documents and we need an abstract model in software to represent these documents, so different types of documents can be addressed via a common interface. In C++, using traditional object orientation techniques, we could end up writing something like this:

```c++
#include <iostream>

static int next_doc_id = 0;

struct doc
{
	virtual ~doc() {}
	virtual void print(std::ostream& os) = 0;
};

struct jpg : doc
{
	jpg() : id(next_doc_id++) {}
	void print(std::ostream& os) override { os << "jpg" << id << "\n"; }
	int id;
};

struct pdf : doc
{
	pdf() : id(next_doc_id++) {}
	void print(std::ostream& os) override { os << "pdf" << id << "\n"; }
	int id;
};

void print_to_stdout(doc& d)
{
	d.print(std::cout);
}

int main()
{
	pdf p;
	print_to_stdout(p);
	jpg j;
	print_to_stdout(j);
	return 0;
}
```
Now imagine that this `print_to_stdout()`-function is often a very slow function which may block other functionality that is more timing sensitive, like responding to a GUI action. In such cases a logical choice to fix this is to offload printing to a different thread. Ideally we would like to define a new function `print_later()` that could look like this - leaving some details unspecified:

```c++
template<class T>
using queue_type = ...;

void print_later(??? d)
{
	extern queue_type<???> print_queue;
	print_queue.push(d);
}
```
`queue_type<T>` is some kind of container to which objects of type `T` can be pushed in a thread-safe way while another dedicated worker thread (or thread pool) pops documents of this queue and prints them. This can be considered as an implementation detail. The main question is, what should we write instead of the `???` ? 

There are many options. We could just pass a `doc` by reference and require clients to keep it alive until really printed, but requiring a potentially simple client to do advanced thread-safe storage management is just very bad API design. Alternatively we can require clients to pass the `doc` wrapped in a smart pointer so we can keep it alive. Or we could copy the instance ourselves to a smart pointer if we make `print_later()` a template function and use the copy constructor of the implementation class. Or if we really want to go old school we could define a virtual `clone()`-function on `doc` to make the copy on the heap. 

Whatever approach is chosen, there is always some extra unwanted boilerplate code required that needs to be written either in the library (the template approach) or either in the client code. Furthermore any decent solution involves a heap allocation. For small objects this already seems stupid intuitively, but this can actually become a performance bottleneck, especially when the allocations and deallocations are done in different threads which is exactly the case in our example. Eventually something that started as a performance improvement could end up as a performance regression. And to make it worse, imagine that half way the refactory of a large legacy project we come across the following:

```c++
struct null_doc : doc
{
	static null_doc& instance() { static null_doc d; return d; }
	void print(std::ostream& os) override { os << "null\n"; }
private:
	null_doc() {}
	null_doc(const null_doc&) = delete;
};

bool is_null(doc& d) { return &d == &null_doc::instance(); }
```

Should we now duplicate the `null_doc` many times on the heap anyway although we know it is constant and re-implement `is_null()` in terms of `dynamic_cast<>`? We could fix this but this example shows that there is no simple once and for all solution.

## The solution
A good solution in this code example would be to replace the `???` with `ddw::impl<doc>`. `ddw::impl` is  generic utility class that can be used to wrap any class instances that implement a particular interface, with the following remarkable properties:
- Can hold an implementation either by value or either by reference. It is up to client to decide that, similar to lambda captures.
- Small object optimization
  - With customizable inline storage. For libraries this is not really applicable, but in application software it is in practice often pretty well known in advance what amount of storage will be needed to hold implementations. Hence heap allocation overhead can be avoided completely if desired.
  - With the possibility to enforce hitting it at compile time. If heap allocations are to be avoided at all cost, innocent size increases of implementation types could potentially break the performance of the application. By enforcing the small object optimization at compile time you're safe. 
- Whatever the exact storage of an implementation is, the library can access it simply by dereferencing the `ddw::impl` object, similar to dereferencing a smart pointer.
- `T&` can be converted implicitly to `ddw::impl<T>` in which case the implementation is passed by reference. Hence, existing APIs can be upgraded partially/gradually without maintenance burden. By replacing `T&` with `ddw::impl<T>`, existing API usages are not broken and not changed semantically, but we do have the possibility to starting passing implementations by value and benefit from the small object optimization.
- `ddw::impl` is moveable, but not copyable. Similar to `std::unique_ptr`.

To give an example, this is how `print_later()` looks like using `ddw::impl`:

```c++
template<class T>
using queue_type = ...;

void print_later(ddw::impl<doc> d)
{
	extern queue_type<ddw::impl<doc>> print_queue;
	print_queue.push(std::move(d));
}
```

and it can be used as follows:

```c++
print_later(null_doc::instance());
print_later(pdf{});
print_later(jpg{});
```

In this example, the `null_doc` will be passed by reference and `pdf{}` and `jpg{}` by value.
Here the choice between passing by value and passing by reference is pretty obvious, but in some situations it can be useful to pass implementations by value or reference explicitly, maybe just to improve the readability of the code. This can be done in an intuitive way:

```c++
print_later(ddw::impl_by_reference(null_doc::instance()));
print_later(ddw::impl_by_value(pdf{}));
print_later(ddw::impl_by_value(jpg{}));
```
 
Putting it all together we have:
- Client-friendly API and passing by reference or by value just works. The client is in control.
- Little or no effort required from library writers to support all this.
- Interface implementations can be passed by value with the possibilty to avoid heap allocations.

## Yet another attempt to fix all OO-problems?
There is a lot of criticism these days on traditional OO programming. The lack of value semantics is only one of them. In some situations, achieving runtime polymorphism through inheritance is already not ideal in the first place. There are solutions available to fix all problems posed by traditional OO programming at once, but that is clearly not what `ddw::impl` tries to do.

`ddw::impl` needs to be seen as a pragmatic solution for some particular real life situations. Let's also not forget that runtime polymorphism through inheritance
- is well supported by the language, think of the `override` keyword for instance.
- is well undertood by the majority of mainstream programmers.

Hence if the disadvantages of inheritance do not apply in a particular situation where a lot of inheritance-based legacy code is involved, but you do need value semantics on interfaces, `ddw::impl` might be the right tool for the job.
