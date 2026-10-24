#pragma once

#include "support.h"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nc = ::nagisa::concurrency;

struct throw_ctrl
{
	bool on_pop_back = false;
	bool on_back = false;
	bool on_empty = false;
};

struct container_boom : ::std::runtime_error
{
	container_boom() : ::std::runtime_error("available container") {}
};

template <class T>
struct throwing_stack
{
	using value_type = T;

	::std::vector<T> impl{};
	throw_ctrl* ctrl = nullptr;

	throwing_stack() = default;
	throwing_stack(::std::vector<T> data, throw_ctrl& ctrl)
		: impl(::std::move(data))
		, ctrl(&ctrl)
	{}

	void fire(bool& flag) const
	{
		if (!flag)
			return;
		flag = false;
		throw container_boom{};
	}

	void push_back(T&& value)
	{
		impl.push_back(::std::move(value));
	}
	T& back()
	{
		if (ctrl)
			fire(ctrl->on_back);
		return impl.back();
	}
	void pop_back()
	{
		if (ctrl)
			fire(ctrl->on_pop_back);
		impl.pop_back();
	}
	[[nodiscard]] bool empty() const
	{
		if (ctrl)
			fire(ctrl->on_empty);
		return impl.empty();
	}
};

using boom_pool = ::nc::resource_pool<int, throwing_stack<int>>;
