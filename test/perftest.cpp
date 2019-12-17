#include "ddw/impl.hpp"
#include <gtest/gtest.h>
#include <boost/lockfree/spsc_queue.hpp>
#include <functional>
#include <thread>
#include <chrono>

using namespace std::literals::chrono_literals;
using namespace std::literals::string_literals;

template<typename T, std::size_t N = 65536, std::size_t B = 256>
struct fifo
{
	typename std::aligned_storage<sizeof(T), alignof(T)>::type data[N];
	uint8_t padding1[64];
	std::size_t head = 0;
	uint8_t padding2[64];
	std::size_t new_head = 0;
	uint8_t padding3[64];
	std::size_t tail = 0;
	uint8_t padding4[64];
	std::size_t new_tail = 0;

	bool full() const
	{
		return (new_tail + 1) % N == head;
	}

	template<typename... Args>
	void push(Args&&... args)
	{
		new (&data[new_tail]) T(std::forward<Args>(args)...);
		new_tail = (new_tail + 1) % N;
		std::size_t taildiff = (N + new_tail - tail) % N;
		if (taildiff >= B) flush();
	}

	void flush()
	{
		atomic_thread_fence(std::memory_order_release);
		tail = new_tail;
	}

	bool empty() const
	{
		return new_head == tail;
	}

	T& front()
	{
		atomic_thread_fence(std::memory_order_acquire);
		return *reinterpret_cast<T*>(&data[new_head]);
	}

	void pop()
	{
		atomic_thread_fence(std::memory_order_acquire);
		reinterpret_cast<T*>(&data[new_head])->~T();
		new_head = (new_head + 1) % N;
		std::size_t headdiff = (N + new_head - head) % N;
		if (headdiff >= B)
		{
			head = new_head;
		}
	}
};

struct msg
{
	virtual ~msg() {}
	virtual void handle() = 0;
};

struct small_capture
{
};

struct medium_capture
{
	int a = 1; int b = 2; int c = 3; int d = 4;
};

struct large_capture
{
	int a = 1; int b = 2; int c = 3; int d = 4;
	int arr[33];
};

template<class extra_capture>
struct count_msg : msg
{
	extra_capture capture;
	int& counter;

	count_msg(int& c) : counter(c) {}

	void handle() override
	{
		counter++;
	}
};

struct done_msg : msg
{
	bool& done;

	done_msg(bool& d) : done(d) {}

	void handle() override
	{
		done = true;
	}
};

static const std::chrono::nanoseconds target_duration = 1s;
static const int interval_count = 10000;

template<typename T>
struct perftest_ctx
{
	std::string description;
	fifo<T> queue;
	bool done = false;
	int counter = 0;

	perftest_ctx(std::string desc) : description(desc) {}

	template<class F1, class F2, class F3>
	void run(F1 post_count, F2 post_done, F3 handle)
	{
		auto t0 = std::chrono::steady_clock::now();

		std::thread writer([&]()
		{
			while (std::chrono::steady_clock::now() - t0 < target_duration)
			{
				for (int i = 0; i < interval_count; i++)
				{
					while (queue.full())
						std::this_thread::sleep_for(1us);
					post_count();
				}
			}
			post_done();
			queue.flush();
		});

		std::thread reader([&]()
		{
			while (not done)
			{
				while (queue.empty())
					std::this_thread::sleep_for(1us);
				handle(queue.front());
				queue.pop();
			}
		});

		reader.join();
		writer.join();

		auto t1 = std::chrono::steady_clock::now();

		std::cout << "fifo holding " << description << " objects processed " << counter * 1s / (t1 - t0) << " msgs per second.\n";
	}
};

TEST(perftest, small_impl_emplace)
{
	perftest_ctx<ddw::impl<msg>> ctx("small emplaced ddw::impl<msg>");
	ctx.run([&ctx]() { ctx.queue.push(ddw::impl_emplace<count_msg<small_capture>>(ctx.counter)); },
			[&ctx]() { ctx.queue.push(done_msg(ctx.done)); },
			[](ddw::impl<msg>& m) { m->handle(); });
}
TEST(perftest, small_std_function)
{
	perftest_ctx<std::function<void()>> ctx("small std::function<void()>");
	ctx.run(
			[&ctx]() { ctx.queue.push([capt = small_capture(), &counter = ctx.counter]() { counter++; }); },
			[&ctx]() { ctx.queue.push([&done = ctx.done]() { done = true; }); },
			[](std::function<void()>& m) { m(); });
}
TEST(perftest, small_std_function_trick)
{
	perftest_ctx<std::function<msg&()>> ctx("small std::function<msg&()>");
	ctx.run(
			[&ctx]() { ctx.queue.push([m = count_msg<small_capture>(ctx.counter)]() mutable -> msg& { return m; }); },
			[&ctx]() { ctx.queue.push([m = done_msg(ctx.done)]() mutable -> msg& { return m; }); },
			[](std::function<msg&()>& m) { m().handle(); });
}
TEST(perftest, small_impl)
{
	perftest_ctx<ddw::impl<msg>> ctx("small ddw::impl<msg>");
	ctx.run([&ctx]() { ctx.queue.push(count_msg<small_capture>(ctx.counter)); },
			[&ctx]() { ctx.queue.push(done_msg(ctx.done)); },
			[](ddw::impl<msg>& m) { m->handle(); });
}
TEST(perftest, medium_std_function)
{
	perftest_ctx<std::function<void()>> ctx("medium std::function<void()>");
	ctx.run([&ctx]() { ctx.queue.push([capt = medium_capture(), &counter = ctx.counter]() { counter++; }); },
			[&ctx]() { ctx.queue.push([&done = ctx.done]() { done = true; }); },
			[](std::function<void()>& m) { m(); });
}
TEST(perftest, medium_std_function_trick)
{
	perftest_ctx<std::function<msg&()>> ctx("medium std::function<msg&()>");
	ctx.run([&ctx]() { ctx.queue.push([m = count_msg<medium_capture>(ctx.counter)]() mutable -> msg& { return m; }); },
			[&ctx]() { ctx.queue.push([m = done_msg(ctx.done)]() mutable -> msg& { return m; }); },
			[](std::function<msg&()>& m) { m().handle(); });
}
TEST(perftest, medium_impl)
{
	perftest_ctx<ddw::impl<msg>> ctx("medium ddw::impl<msg>");
	ctx.run([&ctx]() { ctx.queue.push(count_msg<medium_capture>(ctx.counter)); },
			[&ctx]() { ctx.queue.push(done_msg(ctx.done)); },
			[](ddw::impl<msg>& m) { m->handle(); });
}
TEST(perftest, large_std_function)
{
	perftest_ctx<std::function<void()>> ctx("large std::function<void()>");
	ctx.run([&ctx]() { ctx.queue.push([capt = large_capture(), &counter = ctx.counter]() { counter++; }); },
			[&ctx]() { ctx.queue.push([&done = ctx.done]() { done = true; }); },
			[](std::function<void()>& m) { m(); });
}
TEST(perftest, large_std_function_trick)
{
	perftest_ctx<std::function<msg&()>> ctx("large std::function<msg&()>");
	ctx.run([&ctx]() { ctx.queue.push([m = count_msg<large_capture>(ctx.counter)]() mutable -> msg& { return m; }); },
			[&ctx]() { ctx.queue.push([m = done_msg(ctx.done)]() mutable -> msg& { return m; }); },
			[](std::function<msg&()>& m) { m().handle(); });
}
TEST(perftest, large_impl)
{
	perftest_ctx<ddw::impl<msg>> ctx("large ddw::impl<msg>");
	ctx.run([&ctx]() { ctx.queue.push(count_msg<large_capture>(ctx.counter)); },
			[&ctx]() { ctx.queue.push(done_msg(ctx.done)); },
			[](ddw::impl<msg>& m) { m->handle(); });
}
