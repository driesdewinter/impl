#ifndef IMPL_WRAPPER_HPP_
#define IMPL_WRAPPER_HPP_

#include <type_traits>
#include <utility>
#include <memory>
#include <variant>

namespace ddw
{

namespace detail
{

template<typename T>
struct impl_raw_ptr
{
	using interface_type = T;

	interface_type* get()
	{
		return ptr;
	}

	const interface_type* get() const
	{
		return ptr;
	}

	impl_raw_ptr(interface_type* p = nullptr) : ptr(p) {}

private:
	interface_type* ptr;
};

template<typename T, std::size_t Capacity = 32, std::size_t Alignment = sizeof(void*)>
struct impl_small_value
{
	using interface_type = T;
	static const std::size_t capacity = Capacity;
	static const std::size_t alignment = Alignment;
	using this_type = impl_small_value<interface_type, capacity, alignment>;
    using value_storage = typename std::aligned_storage<capacity, alignment>::type;

    template<typename U>
	struct cbs
	{
		using impl_type = U;

		static void move(value_storage* dst, value_storage* src)
		{
		    new (dst) impl_type(reinterpret_cast<impl_type&&>(*src));
		}
	};

    interface_type* get()
    {
    	return reinterpret_cast<interface_type*>(&value);
    }

    const interface_type* get() const
    {
    	return reinterpret_cast<const interface_type*>(&value);
    }

    impl_small_value() = delete;
    impl_small_value(const this_type&) = delete;

    template<typename U>
    impl_small_value(U&& v)
    {
    	using impl_type = std::remove_const_t<std::remove_reference_t<U>>;
    	move = cbs<impl_type>::move;
	    new (&value) impl_type(std::forward<U>(v));
    }

    template<typename U, typename... Args>
    impl_small_value(U*, Args&&... args)
    {
    	using impl_type = U;
    	move = cbs<impl_type>::move;
	    new (&value) impl_type(std::forward<Args>(args)...);
    }

    template<typename U, std::size_t C, std::size_t A>
    impl_small_value(impl_small_value<U, C, A>&& other)
    {
    	move = other.move;
    	move(&value, &other.value);
    }

    ~impl_small_value()
    {
    	get()->~interface_type();
    }

private:
	void (*move)(value_storage* dst, value_storage* src);
    value_storage value;
};

template<typename U>
struct impl_forced_value
{
	U&& v;
};

template<typename U>
struct impl_forced_small_value
{
	U&& v;
};

template<typename U>
struct impl_forced_reference
{
	U&& v;
};

template<typename U, typename... Args>
struct impl_emplacement
{
	using impl_type = U;
	std::tuple<Args&&...> args;
};

template<typename U, typename... Args>
struct impl_emplacement_small
{
	using impl_type = U;
	std::tuple<Args&&...> args;
};

}

template<typename T, std::size_t Capacity = 32, std::size_t Alignment = sizeof(void*)>
class impl
{
public:
	using interface_type = T;
	static const std::size_t capacity = Capacity;
	static const std::size_t alignment = Alignment;
	using this_type = impl<interface_type, capacity, alignment>;

	impl() = default;

	template<typename U>
	impl(U&& v)
	{
		auto_selector<U>::reset(this, std::forward<U>(v));
	}

	template<typename U>
	this_type& operator=(U&& v)
	{
		auto_selector<U>::reset(this, std::forward<U>(v));
		return *this;
	}

	interface_type& operator*()
	{
		return *get();
	}

	const interface_type& operator*() const
	{
		return *get();
	}

	interface_type* operator->()
	{
		return get();
	}

	const interface_type* operator->() const
	{
		return get();
	}

	operator bool() const
	{
		return has_impl();
	}

	template<typename U>
	void reset(U&& v)
	{
		auto_selector<U>::reset(this, std::forward<U>(v));
	}

	template<typename U>
	void reset_reference(U& v)
	{
    	using impl_type = std::remove_const_t<std::remove_reference_t<U>>;
	    static_assert(std::is_base_of_v<T, impl_type>, "T is not a base of U");
		_d.template emplace<raw_type>(&v);
	}

	template<typename U>
	void reset_value(U&& v)
	{
    	using impl_type = std::remove_const_t<std::remove_reference_t<U>>;
	    static_assert(std::is_base_of_v<T, impl_type>, "T is not a base of U");
	    if constexpr (sizeof(impl_type) <= capacity and alignof(impl_type) <= alignment)
	    	reset_small_value(std::forward<U>(v));
	    else
	    	reset_big_value(std::forward<U>(v));
	}

	template<typename U>
	void reset_small_value(U&& v)
	{
    	using impl_type = std::remove_const_t<std::remove_reference_t<U>>;
	    static_assert(std::is_base_of_v<T, impl_type>, "T is not a base of U");
	    static_assert(sizeof(impl_type) <= capacity, "capacity too small to store U");
	    static_assert(alignof(impl_type) <= alignment, "alignment too small to store U");
		_d.template emplace<small_type>(std::forward<U>(v));
	}

	template<typename U>
	void reset_big_value(U&& v)
	{
    	using impl_type = std::remove_const_t<std::remove_reference_t<U>>;
	    static_assert(std::is_base_of_v<T, impl_type>, "T is not a base of U");
		_d.template emplace<unique_type>(std::make_unique<impl_type>(std::forward<U>(v)));
	}

	template<typename U>
	void reset_unique(std::unique_ptr<U>&& v)
	{
	    static_assert(std::is_base_of_v<T, U>, "T is not a base of U");
		_d.template emplace<unique_type>(std::move(v));
	}

	template<typename U>
	void reset_shared(std::shared_ptr<U>&& v)
	{
	    static_assert(std::is_base_of_v<T, U>, "T is not a base of U");
		_d.template emplace<shared_type>(std::move(v));
	}

	template<typename U, std::size_t C, std::size_t A>
	void reset_impl(impl<U, C, A>&& v)
	{
	    static_assert(std::is_base_of_v<T, U>, "T is not a base of U");
		std::visit([this](auto &d) {
			auto_selector<std::remove_reference_t<decltype(d)>>::reset(this, std::move(d));
		}, v._d);
	}

	template<typename U, typename... Args>
	void emplace(Args&&... args)
	{
	    static_assert(std::is_base_of_v<T, U>, "T is not a base of U");
	    if constexpr (sizeof(U) <= capacity and alignof(U) <= alignment)
	    	emplace_small<U>(std::forward<Args>(args)...);
	    else
	    	emplace_big<U>(std::forward<Args>(args)...);
	}

	template<typename U, typename... Args>
	void emplace_small(Args&&... args)
	{
	    static_assert(std::is_base_of_v<T, U>, "T is not a base of U");
	    static_assert(sizeof(U) <= capacity, "capacity too small to store U");
	    static_assert(alignof(U) <= alignment, "alignment too small to store U");
		_d.template emplace<small_type>(static_cast<U*>(nullptr), std::forward<Args>(args)...);
	}

	template<typename U, typename... Args>
	void emplace_big(Args&&... args)
	{
	    static_assert(std::is_base_of_v<T, U>, "T is not a base of U");
		_d.template emplace<unique_type>(std::make_unique<U>(std::forward<Args>(args)...));
	}

	interface_type* get()
	{
		return std::visit([](auto& d) { return d.get(); }, _d);
	}

	const interface_type* get() const
	{
		return std::visit([](const auto& d) { return d.get(); }, _d);
	}

	bool has_impl() const
	{
		return get() != nullptr;
	}

private:
	using raw_type = detail::impl_raw_ptr<interface_type>;
	using unique_type = std::unique_ptr<interface_type>;
	using small_type = detail::impl_small_value<interface_type, capacity, alignment>;
	using shared_type = std::shared_ptr<interface_type>;

	template<typename U, std::size_t C, std::size_t A>
	void reset_impl_small_value(detail::impl_small_value<U, C, A>&& v)
	{
    	using impl_type = U;
	    static_assert(std::is_base_of_v<T, impl_type>, "T is not a base of impl_type");
	    static_assert(C <= capacity, "capacity too small to store impl_type");
	    static_assert(A <= alignment, "alignment too small to store impl_type");
		_d.template emplace<small_type>(std::move(v));
	}

	template<typename U, typename = void>
	struct auto_selector {};

	template<typename U>
	struct auto_selector<U&, std::enable_if_t<
			std::is_base_of_v<interface_type, U>
			and not std::is_const_v<U> and not std::is_copy_constructible_v<U>>>
	{
		static void reset(this_type* pthis, U& v)
		{
			pthis->reset_reference(v);
		}
	};

	template<typename U>
	struct auto_selector<U, std::enable_if_t<
			std::is_base_of_v<interface_type, U>
			and (std::is_copy_constructible_v<U> or std::is_move_constructible_v<U>)>>
	{
		static void reset(this_type* pthis, U&& v)
		{
			pthis->reset_value(std::move(v));
		}
	};

	template<typename U>
	struct auto_selector<U&, std::enable_if_t<
			std::is_base_of_v<interface_type, U>
			and std::is_copy_constructible_v<U>>>
	{
		static void reset(this_type* pthis, U& v)
		{
			pthis->reset_value(v);
		}
	};

	template<typename U>
	struct auto_selector<std::unique_ptr<U>>
	{
		static void reset(this_type* pthis, std::unique_ptr<U>&& v)
		{
			pthis->reset_unique(std::move(v));
		}
	};

	template<typename U>
	struct auto_selector<std::shared_ptr<U>>
	{
		static void reset(this_type* pthis, std::shared_ptr<U> v)
		{
			pthis->reset_shared(std::move(v));
		}
	};

	template<typename U, std::size_t C, std::size_t A>
	struct auto_selector<impl<U, C, A>>
	{
		static void reset(this_type* pthis, impl<U, C, A>&& v)
		{
			pthis->reset_impl(std::move(v));
		}
	};

	template<typename U, std::size_t C, std::size_t A>
	struct auto_selector<detail::impl_small_value<U, C, A>>
	{
		static void reset(this_type* pthis, detail::impl_small_value<U, C, A>&& v)
		{
			pthis->reset_impl_small_value(std::move(v));
		}
	};

	template<typename U>
	struct auto_selector<detail::impl_raw_ptr<U>>
	{
		static void reset(this_type* pthis, detail::impl_raw_ptr<U>&& v)
		{
			pthis->reset_reference(*v.get());
		}
	};

	template<typename U>
	struct auto_selector<detail::impl_forced_value<U>>
	{
		static void reset(this_type* pthis, detail::impl_forced_value<U>&& v)
		{
			pthis->reset_value(std::forward<U>(v.v));
		}
	};

	template<typename U>
	struct auto_selector<detail::impl_forced_small_value<U>>
	{
		static void reset(this_type* pthis, detail::impl_forced_small_value<U>&& v)
		{
			pthis->reset_small_value(std::forward<U>(v.v));
		}
	};

	template<typename U>
	struct auto_selector<detail::impl_forced_reference<U>>
	{
		static void reset(this_type* pthis, detail::impl_forced_reference<U>&& v)
		{
			pthis->reset_reference(std::forward<U>(v.v));
		}
	};

	template<typename U, typename... Args>
	struct auto_selector<detail::impl_emplacement<U, Args...>>
	{
		static void reset(this_type* pthis, detail::impl_emplacement<U, Args...>&& v)
		{
			std::apply([pthis](Args&&... args) {
				pthis->template emplace<U>(std::forward<Args>(args)...);
			}, std::move(v.args));
		}
	};

	template<typename U, typename... Args>
	struct auto_selector<detail::impl_emplacement_small<U, Args...>>
	{
		static void reset(this_type* pthis, detail::impl_emplacement_small<U, Args...>&& v)
		{
			std::apply([pthis](Args&&... args) {
				pthis->template emplace_small<U>(std::forward<Args>(args)...);
			}, std::move(v.args));
		}
	};

    std::variant<raw_type, small_type, unique_type, shared_type> _d;
};

template<typename U>
auto impl_by_value(U&& v)
{
	return detail::impl_forced_value<U>{std::forward<U>(v)};
}

template<typename U>
auto impl_by_small_value(U&& v)
{
	return detail::impl_forced_small_value<U>{std::forward<U>(v)};
}

template<typename U>
auto impl_by_reference(U&& v)
{
	return detail::impl_forced_reference<U>{std::forward<U>(v)};
}

template<typename U, typename... Args>
auto impl_emplace(Args&&... args)
{
	return detail::impl_emplacement<U, Args...>{std::forward_as_tuple(args...)};
}

template<typename U, typename... Args>
auto impl_emplace_small(Args&&... args)
{
	return detail::impl_emplacement_small<U, Args...>{std::forward_as_tuple(args...)};
}

}

#endif
