#pragma once

/// @file task.h
/// @brief Two minimal coroutine return-object templates:
///        @ref basic_fork (non-owning) and @ref basic_task (owning).
///
/// They are intentionally lightweight: just a wrapper around the
/// coroutine handle, plus optional @c operator @c co_await /
/// @c as_awaitable hooks. Behavior such as eager/lazy start, value
/// retrieval, scheduler propagation, etc. lives entirely in the
/// @c Promise and @c Awaitable template parameters — see the
/// component library (component/*.h) and @ref awaitable_trait_combiner.

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

/// @brief Placeholder template indicating "no awaitable adapter".
///
/// When a @c basic_task / @c basic_fork is instantiated with this
/// placeholder, the type does *not* satisfy @ref awaitable — it acts
/// purely as a handle wrapper.
template<class,class>
struct noop_awaitable;

template<class Promise, template<class, class>class Awaitable = noop_awaitable>
struct basic_fork;


/// @brief Non-owning coroutine wrapper (handle-only specialization).
///
/// Holds a @c std::coroutine_handle<Promise> but does **not** destroy
/// the coroutine in its destructor. Use this as the base building
/// block when the lifetime is managed elsewhere (for example, by a
/// parent @c basic_task that holds the same handle).
///
/// @tparam Promise The coroutine's promise type.
template<class Promise>
struct basic_fork<Promise, noop_awaitable>
{
private:
	using self_type = basic_fork;
public:
	using promise_type = Promise;
	using handle_type = ::std::coroutine_handle<promise_type>;

	/// @brief Wraps an existing coroutine handle.
	constexpr explicit(false) basic_fork(handle_type const coroutine) noexcept : _coroutine(coroutine) {}

	/// @brief Returns the underlying coroutine handle (may be null).
	constexpr auto handle() const noexcept { return _coroutine; }

	handle_type _coroutine;
};

/// @brief Non-owning coroutine wrapper with an awaitable adapter.
///
/// Extends @ref basic_fork by exposing the coroutine as an awaitable:
/// you can write <tt>co_await some_fork</tt> from another coroutine.
///
/// Because this overload is non-owning, awaiting it does **not** transfer
/// or reset the handle — the same coroutine can be awaited multiple
/// times (subject to whatever the awaitable adapter permits).
///
/// @tparam Promise   The coroutine's promise type.
/// @tparam Awaitable A class template <tt>Awaitable&lt;Promise, ParentPromise&gt;</tt>
///                   constructible from @c handle_type
///                   (and optionally from <tt>handle_type, ParentPromise&amp;</tt>).
///                   Use @ref build_awaitable_t to assemble one from traits.
template<class Promise, template<class, class>class Awaitable>
struct basic_fork : basic_fork<Promise, noop_awaitable>
{
private:
	using self_type = basic_fork;
	using base_type = basic_fork<Promise, noop_awaitable>;
public:
	using promise_type = typename base_type::promise_type;
	using handle_type = typename base_type::handle_type;
	template<class ParentPromise = void>
	using awaitable_type = Awaitable<promise_type, ParentPromise>;

	using base_type::base_type;

	/// @brief Lets the object appear after @c co_await.
	///
	/// Constructs the awaitable adapter from the (non-released) handle.
	constexpr auto operator co_await() noexcept
		requires ::std::constructible_from<awaitable_type<void>, handle_type>
	{
		return awaitable_type<void>{base_type::handle()};
	}

	/// @brief Adapts this fork to be awaited inside a specific parent promise.
	///
	/// Used by promise types whose @c await_transform forwards through
	/// @c as_awaitable. Picks the 2-argument adapter constructor if
	/// available, otherwise the 1-argument one.
	template<class ParentPromise>
		requires ::std::constructible_from<awaitable_type<ParentPromise>, handle_type>
		|| ::std::constructible_from<awaitable_type<ParentPromise>, handle_type, ParentPromise&>
	constexpr auto as_awaitable(ParentPromise& promise) noexcept
	{
		using awaitable_type = awaitable_type<ParentPromise>;
		if constexpr (::std::constructible_from<awaitable_type, handle_type, ParentPromise&>)
			return awaitable_type(base_type::handle(), promise);
		else
			return awaitable_type(base_type::handle());
	}
};

template<class Promise, template<class, class>class Awaitable = noop_awaitable>
struct basic_task;

/// @brief Owning coroutine wrapper (handle-only specialization).
///
/// RAII container: destroys the underlying coroutine when the task
/// goes out of scope. Move-only — you cannot duplicate ownership, and
/// move-assignment is disabled to avoid silently leaking the
/// previously-held coroutine.
///
/// Call @c release() to surrender ownership (the caller becomes
/// responsible for calling @c handle.destroy() eventually).
///
/// @tparam Promise The coroutine's promise type.
template<class Promise>
struct basic_task<Promise, noop_awaitable> : basic_fork<Promise, noop_awaitable>
{
private:
	using self_type = basic_task;
	using base_type = basic_fork<Promise, noop_awaitable>;
public:
	using base_type::base_type;
	constexpr basic_task(self_type const&) noexcept = delete;
	constexpr self_type& operator=(self_type const&) noexcept = delete;
	/// @brief Move-construct, transferring the handle and leaving @p other empty.
	constexpr basic_task(self_type&& other) noexcept : base_type(other.release()) {}
	/// @brief Move-assignment is disabled to prevent silently leaking the held coroutine.
	constexpr self_type& operator=(self_type&&) noexcept = delete;
	/// @brief Destroys the coroutine frame if still owned.
	constexpr ~basic_task() noexcept
	{
		if (base_type::handle())
			base_type::handle().destroy();
	}
	/// @brief Surrenders ownership and returns the handle.
	///
	/// After the call, the task is empty and its destructor is a no-op;
	/// the caller takes full responsibility for eventually destroying
	/// the coroutine (typically by resuming it to completion under a
	/// promise that self-destructs on @c final_suspend, see
	/// @c promises::exit_then_destroy).
	constexpr auto release() noexcept { return ::std::exchange(base_type::_coroutine, nullptr); }
};

/// @brief Owning coroutine wrapper with an awaitable adapter.
///
/// Awaiting a @c basic_task **moves the handle out** (@c operator @c co_await
/// is rvalue-qualified and calls @c release()). That means the awaiter
/// now owns the coroutine; the task itself becomes empty.
///
/// Typical usage:
/// @code
///   simple_task<int> t = make_task();
///   int r = co_await std::move(t);   // ownership transferred to the awaiter
/// @endcode
///
/// or implicitly when @c t is a prvalue: @c co_await @c make_task() works
/// without an explicit @c std::move.
///
/// @tparam Promise   The coroutine's promise type.
/// @tparam Awaitable A class template <tt>Awaitable&lt;Promise, ParentPromise&gt;</tt>
///                   constructible from @c handle_type
///                   (and optionally from <tt>handle_type, ParentPromise&amp;</tt>).
template<class Promise, template<class, class>class Awaitable>
struct basic_task : basic_task<Promise, noop_awaitable>
{
private:
	using self_type = basic_task;
	using base_type = basic_task<Promise, noop_awaitable>;
public:
	using promise_type = typename base_type::promise_type;
	using handle_type = typename base_type::handle_type;
	template<class ParentPromise = void>
	using awaitable_type = Awaitable<promise_type, ParentPromise>;

	using base_type::base_type;

	/// @brief Lets the task appear after @c co_await; transfers ownership.
	///
	/// Only callable on an rvalue. The awaiter takes the handle out via
	/// @c release(), so the source task is left empty.
	constexpr auto operator co_await() && noexcept
		requires ::std::constructible_from<awaitable_type<void>, handle_type>
	{
		return awaitable_type<void>(base_type::release());
	}

	/// @brief Adapts this task to be awaited inside a specific parent promise.
	///
	/// Same ownership semantics as @c operator @c co_await — the handle
	/// is transferred to the returned awaitable.
	template<class ParentPromise>
		requires ::std::constructible_from<awaitable_type<ParentPromise>, handle_type>
		|| ::std::constructible_from<awaitable_type<ParentPromise>, handle_type, ParentPromise&>
	constexpr auto as_awaitable(ParentPromise& promise) && noexcept
	{
		using awaitable_type = awaitable_type<ParentPromise>;
		if constexpr(::std::constructible_from<awaitable_type, handle_type, ParentPromise&>)
			return awaitable_type(base_type::release(), promise);
		else
			return awaitable_type(base_type::release());
	}
};

NAGISA_BUILD_LIB_DETAIL_END
