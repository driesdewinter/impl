#include <iostream>
#include <thread>
#include <chrono>
#include "fifo.h"
#include "ddw/impl.hpp"

using namespace std::literals::chrono_literals;

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

template<class T>
using queue_type = fifo<T, 100, 1>;

void print_later(ddw::impl<doc> d)
{
	extern queue_type<ddw::impl<doc>> print_queue;
	print_queue.push(std::move(d));
}

struct null_doc : doc
{
	static null_doc& instance() { static null_doc d; return d; }
	void print(std::ostream& os) override { os << "null\n"; }
private:
	null_doc() {}
	null_doc(const null_doc&) = delete;
};

bool is_null(doc& d) { return &d == &null_doc::instance(); }

fifo<ddw::impl<doc>, 100, 1> print_queue;

int main()
{
	bool done = false;
	std::thread print_thread([&done]() {
		while (not done or not print_queue.empty())
		{
			while (print_queue.empty())
				std::this_thread::sleep_for(1us);
			if (not is_null(*print_queue.front())) print_queue.front()->print(std::cout);
			print_queue.pop();
		}
	});

	print_later(null_doc::instance());
	print_later(pdf{});
	print_later(jpg{});

	done = true;
	print_thread.join();

	return 0;
}
