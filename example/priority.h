#pragma once

#include <chrono>
#include <coroutine>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <concepts>
#include <ranges>

template<class Container>
struct ordered_container_trait_base
{
	using container_type = Container;

	constexpr static decltype(auto) emplace(::std::convertible_to<container_type const&> auto&& container, auto&&... args)
		noexcept(::std::forward<decltype(container)>(container).emplace(::std::forward<decltype(args)>(args)...))
		requires requires{ ::std::forward<decltype(container)>(container).emplace(::std::forward<decltype(args)>(args)...); }
	{
		return ::std::forward<decltype(container)>(container).emplace(::std::forward<decltype(args)>(args)...);
	}
	constexpr static decltype(auto) empty(::std::convertible_to<container_type const&> auto&& container)
		noexcept(::std::forward<decltype(container)>(container).empty())
		requires requires{ ::std::forward<decltype(container)>(container).empty(); }
	{
		return ::std::forward<decltype(container)>(container).empty();
	}
	constexpr static decltype(auto) front(::std::convertible_to<container_type const&> auto&& container)
		noexcept(::std::forward<decltype(container)>(container).front())
		requires requires{ ::std::forward<decltype(container)>(container).front(); }
	{
		return ::std::forward<decltype(container)>(container).front();
	}
	constexpr static decltype(auto) pop_front(::std::convertible_to<container_type const&> auto&& container)
		noexcept(::std::forward<decltype(container)>(container).pop_front())
		requires requires{ ::std::forward<decltype(container)>(container).pop_front(); }
	{
		return ::std::forward<decltype(container)>(container).pop_front();
	}
};
template<class Container>
struct ordered_container_trait : ordered_container_trait_base<Container> {};

namespace std { template<class Key, class Value, class Compare, class Alloc> class map; }

template<class Key, class Value, class Compare, class Alloc>
struct ordered_container_trait<::std::map<Key, Value, Compare, Alloc>>
	: ordered_container_trait_base<::std::map<Key, Value, Compare, Alloc>>
{
	using container_type = ::std::map<Key, Value, Compare, Alloc>;
	using base_type = ordered_container_trait_base<container_type>;
	using base_type::emplace;
	using base_type::empty;

	constexpr static decltype(auto) front(container_type& container) noexcept { return *container.begin(); }
	constexpr static decltype(auto) pop_front(container_type& container) noexcept { return container.erase(container.begin()); }
};


namespace std { template<class Key, class Value, class Compare, class Alloc> class multimap; }

template<class Key, class Value, class Compare, class Alloc>
struct ordered_container_trait<::std::multimap<Key, Value, Compare, Alloc>>
	: ordered_container_trait_base<::std::multimap<Key, Value, Compare, Alloc>>
{
	using container_type = ::std::multimap<Key, Value, Compare, Alloc>;
	using base_type = ordered_container_trait_base<container_type>;
	using base_type::emplace;
	using base_type::empty;

	constexpr static decltype(auto) front(container_type& container) noexcept { return *container.begin(); }
	constexpr static decltype(auto) pop_front(container_type& container) noexcept { return container.erase(container.begin()); }
};

namespace std { template<class T, class Container, class Compare> class priority_queue; }

template<class T, class Container, class Compare>
struct ordered_container_trait<::std::priority_queue<T, Container, Compare>>
	: ordered_container_trait_base<::std::priority_queue<T, Container, Compare>>
{
	using container_type = ::std::priority_queue<T, Container, Compare>;
	using base_type = ordered_container_trait_base<container_type>;
	using base_type::emplace;
	using base_type::empty;

	constexpr static decltype(auto) front(container_type& container) noexcept { return container.top(); }
	constexpr static decltype(auto) pop_front(container_type& container) noexcept { return container.pop(); }
};


#include <map>
#include <queue>

struct priority_queue
{
	using Container = ::std::multimap<::std::chrono::steady_clock::time_point, ::std::coroutine_handle<>>;
	using container_type = Container;
	using trait_type = ordered_container_trait<container_type>;

	void emplace(auto&&... args)
		noexcept(trait_type::emplace(::std::declval<container_type>(), ::std::forward<decltype(args)>(args)...))
		requires requires(container_type container)
	{
		trait_type::emplace(container, ::std::forward<decltype(args)>(args)...);
	}
	{
		auto lk = ::std::unique_lock(_mutex);
		trait_type::emplace(_container, ::std::forward<decltype(args)>(args)...);
		_cv.notify_one();
	}
	void emplace_maybe_wakeup(auto should_wakeup, auto&&... args)
		noexcept(trait_type::emplace(::std::declval<container_type>(), ::std::forward<decltype(args)>(args)...))
		requires requires(container_type container)
	{
		trait_type::emplace(container, ::std::forward<decltype(args)>(args)...);
		{ should_wakeup(trait_type::front(container), ::std::forward<decltype(args)>(args)...) } -> ::std::convertible_to<bool>;
	}
	{
		auto lk = ::std::unique_lock(_mutex);
		auto wakeup = should_wakeup(trait_type::front(_container), ::std::forward<decltype(args)>(args)...);
		trait_type::emplace(_container, ::std::forward<decltype(args)>(args)...);
		if (wakeup)
			_cv.notify_one();
	}
	decltype(auto) pop(auto ready) noexcept
	{
		auto lk = ::std::unique_lock(_mutex);
		while (true)
		{
			if (trait_type::empty(_container))
				_cv.wait(lk, [this] { return !trait_type::empty(_container); });
			auto&& value = trait_type::front(_container);
			if (ready(value))
			{
				auto result = ::std::move(value);
				trait_type::pop_front(_container);
				return result;
			}
			auto status = _cv.wait_until(lk, task.due);
		}
	}

	::std::optional<task_type> pop(::stdexec::stoppable_token auto const& stop_token) noexcept
	{
		auto lk = ::std::unique_lock(_mutex);
		auto notify = [this] { _cv.notify_all(); };
		auto callback = ::stdexec::stop_callback_for_t<::std::remove_cvref_t<decltype(stop_token)>, decltype(notify)>(stop_token, notify);
		while (true)
		{
			if (_tasks.empty())
				_cv.wait(lk, [this, &stop_token] { return stop_token.stop_requested() || !_tasks.empty(); });
			if (stop_token.stop_requested())
				return ::std::nullopt;
			auto task = _tasks.top();
			if (task.due <= clock_type::now())
			{
				_tasks.pop();
				return task;
			}
			auto status = _cv.wait_until(lk, task.due);
			if (stop_token.stop_requested())
				return ::std::nullopt;
		}
	}

	::std::mutex _mutex{};
	::std::condition_variable _cv{};
	container_type _container{};
};