#pragma once

#include <memory>
#include <type_traits>
#include <utility>

struct move_only_slot
{
	int id = -1;

	move_only_slot() = default;
	explicit move_only_slot(int id) noexcept : id(id) {}
	move_only_slot(move_only_slot const&) = delete;
	move_only_slot& operator=(move_only_slot const&) = delete;
	move_only_slot(move_only_slot&& other) noexcept : id(::std::exchange(other.id, -1)) {}
	move_only_slot& operator=(move_only_slot&& other) noexcept
	{
		id = ::std::exchange(other.id, -1);
		return *this;
	}
};

static_assert(!::std::is_copy_constructible_v<move_only_slot>);
static_assert(!::std::is_copy_assignable_v<move_only_slot>);
static_assert(::std::is_nothrow_move_constructible_v<move_only_slot>);
static_assert(::std::is_nothrow_move_assignable_v<move_only_slot>);

using unique_int = ::std::unique_ptr<int>;
