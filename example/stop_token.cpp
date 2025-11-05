#include <iostream>
#include <coroutine>

#include <nagisa/concurrency/concurrency.h>
#include <exec/any_sender_of.hpp>
#include <exec/task.hpp>
namespace nc = ::nagisa::concurrency;

using task = ::nc::simple_task<void, false>;

task f1() noexcept
{
	auto token = co_await ::stdexec::get_stop_token();
	::std::cout << ::std::boolalpha << token.stop_requested() << '\n';
}

task f2() noexcept
{
	auto token = co_await ::stdexec::get_stop_token();
	::std::cout << ::std::boolalpha << token.stop_requested() << '\n';
	co_await ::std::suspend_always{};
	::std::cout << ::std::boolalpha << token.stop_requested() << '\n';
	co_await ::f1();
	::std::cout << ::std::boolalpha << token.stop_requested() << '\n';
}

task g2(auto&&) noexcept
{
	auto token = co_await ::stdexec::get_stop_token();
	::std::cout << ::std::boolalpha << token.stop_requested() << '\n';
	co_await ::std::suspend_always{};
	::std::cout << ::std::boolalpha << token.stop_requested() << '\n';
	co_await ::f1();
	::std::cout << ::std::boolalpha << token.stop_requested() << '\n';
}

struct my_stop_token : ::std::stop_token
{
	template<class C>
	using callback_type = ::std::stop_callback<C>;

	using ::std::stop_token::stop_token;
	my_stop_token& operator=(::std::stop_token&& st) noexcept
	{
		::std::stop_token::operator=(::std::move(st));
		return *this;
	}
};
using task2 = ::nc::simple_task<void, false, ::stdexec::inline_scheduler, ::nc::intro_type::lazy, my_stop_token>;

task2 h2() noexcept
{
	auto token = co_await ::stdexec::get_stop_token();
	::std::cout << ::std::boolalpha << token.stop_requested() << '\n';
	co_await ::std::suspend_always{};
	::std::cout << ::std::boolalpha << token.stop_requested() << '\n';
	co_await ::f1().operator co_await();
	::std::cout << ::std::boolalpha << token.stop_requested() << '\n';
}

template<class ST>
struct env
{
	constexpr auto query(::stdexec::get_stop_token_t) const noexcept { return st; }
	ST st;
};

int main()
{
	{
		::stdexec::inplace_stop_source ss{};
		auto task = f2();
		::nc::set_stop_token(task.handle().promise(), ss.get_token());
		task.handle()();
		ss.request_stop();
		task.handle()();
	}
	{
		::stdexec::inplace_stop_source ss{};
		auto task = g2(env{ss.get_token()});
		task.handle()();
		ss.request_stop();
		task.handle()();
	}
	{
		::std::stop_source ss{};
		auto task = h2();
		task.handle().promise().set_stop_token(ss.get_token());
		::nc::set_stop_token(task.handle().promise(), ss.get_token());
		task.handle()();
		ss.request_stop();
		task.handle()();
	}

	return 0;
}
