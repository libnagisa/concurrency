#pragma once

/// @file awaitable.h
/// @brief Defines the @c awaitable concept and the @c get_awaiter helpers.
///
/// While an *awaiter* (see awaiter.h) is what @c co_await directly drives,
/// an *awaitable* is any expression you can write after @c co_await — even
/// if it isn't an awaiter itself. The standard performs up to two
/// conversions to obtain an awaiter from an awaitable:
///   1. If the surrounding promise defines @c await_transform(e), use it.
///   2. Otherwise, if @c e has a member or non-member @c operator @c co_await,
///      call it.
///   3. Otherwise, @c e itself must already be an awaiter.
///
/// @c get_awaiter performs exactly that chain so library code can reason
/// about "the awaiter that @c co_await would actually use".

#include "./awaiter.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN


#if defined(_MSC_VER) && !defined(__clang__)
// MSVCBUG https://developercommunity.visualstudio.com/t/operator-co_await-not-found-in-requires/10452721

/// @internal Workaround for an MSVC bug where @c requires can't see free
/// @c operator @c co_await directly. We instead probe via a constrained
/// function declaration.
template <class Awaitable>
void co_await_constraint(Awaitable&& a) noexcept
	requires requires { operator co_await(static_cast<Awaitable&&>(a)); };
#endif

/// @brief Obtains the awaiter for @p a as if performing @c co_await @p a
///        outside of any coroutine (i.e. without consulting @c await_transform).
///
/// Resolution order:
///   1. Member @c operator @c co_await
///   2. Non-member (ADL) @c operator @c co_await
///   3. @p a itself
template <class Awaitable>
decltype(auto) get_awaiter(Awaitable&& a)
{
	if constexpr (requires { static_cast<Awaitable&&>(a).operator co_await(); }) {
		return static_cast<Awaitable&&>(a).operator co_await();
	}
	else if constexpr (requires {
#if defined(_MSC_VER) && !defined(__clang__)
		co_await_constraint(static_cast<Awaitable&&>(a));
#else
		operator co_await(static_cast<Awaitable&&>(a));
#endif
	}) {
		return operator co_await(static_cast<Awaitable&&>(a));
	}
	else {
		return static_cast<Awaitable&&>(a);
	}
}


/// @brief Obtains the awaiter for @p a as if performing @c co_await @p a
///        inside a coroutine whose promise is @p p.
///
/// If @p p defines @c await_transform, it is applied first; the result
/// is then passed through the no-promise overload of @ref get_awaiter.
template <class Awaitable, class Promise>
decltype(auto) get_awaiter(Awaitable&& a, Promise& p)
{
	if constexpr (requires { p->await_transform(details::get_awaiter(static_cast<Awaitable&&>(a))); })
	{
		return p.await_transform(details::get_awaiter(static_cast<Awaitable&&>(a)));
	}
	else
	{
		return details::get_awaiter(static_cast<Awaitable&&>(a));
	}
}

/// @brief Matches any expression that can appear after @c co_await
///        inside a coroutine whose promise is @p ParentPromise.
///
/// When @p ParentPromise is @c void, the promise step is skipped — the
/// type is awaitable in any context that doesn't need @c await_transform.
///
/// @tparam Awaitable     The candidate expression type.
/// @tparam ParentPromise The promise type of the surrounding coroutine,
///         or @c void to leave it unconstrained.
template <class Awaitable, class ParentPromise = void>
concept awaitable = (::std::same_as<ParentPromise, void> && requires(Awaitable && a)
{
	{ details::get_awaiter(static_cast<Awaitable&&>(a)) } -> awaiter<ParentPromise>;
}) || requires(Awaitable && a, ParentPromise& p)
{
	{ details::get_awaiter(static_cast<Awaitable&&>(a), p) } -> awaiter<ParentPromise>;
};

/// @brief Metafunction yielding the awaiter type for @p T inside promise @p P.
/// @see awaitable_awaiter_t
template<class T, class P = void> requires awaitable<T, P>
struct awaitable_awaiter
{
	using type = decltype(details::get_awaiter(::std::declval<T>(), ::std::declval<P>()));
};
template<awaitable T>
struct awaitable_awaiter<T, void>
{
	using type = decltype(details::get_awaiter(::std::declval<T>()));
};

/// @brief Alias for the awaiter type of @p T inside promise @p P.
///
/// Equivalent to <tt>decltype(get_awaiter(declval<T>(), declval<P>()))</tt>.
template<class T, class P = void> requires awaitable<T, P>
using awaitable_awaiter_t = typename awaitable_awaiter<T, P>::type;


NAGISA_BUILD_LIB_DETAIL_END
