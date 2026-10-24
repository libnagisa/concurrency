#pragma once

#include <concepts>

#include "./environment.h"


NAGISA_BUILD_LIB_DETAIL_BEGIN

template <class Node>
concept queue_node = requires(Node & n) {
	{ n._next } -> ::std::convertible_to<Node*>;
};

// 纯侵入式单链表，FIFO 入队/出队 + O(n) unlink。不持有任何锁——
// 调用方已经在自己的锁内才会碰这个结构，这里不重复加锁、不重复定义
// "完成时传什么值"（那由 Node 自己的完成点决定，这个类型从不调用它）。
template <queue_node Node>
struct wait_queue
{
private:
	using self_type = wait_queue;
public:
	using node_type = Node;

	node_type* _head = nullptr;
	node_type* _tail = nullptr;

	[[nodiscard]] constexpr bool empty() const noexcept { return _head == nullptr; }

	constexpr void push_back(node_type& node) noexcept
	{
		node._next = nullptr;
		if (_tail == nullptr)
			_head = &node;
		else
			_tail->_next = &node;
		_tail = &node;
	}
	[[nodiscard]] constexpr node_type* pop_front() noexcept
	{
		auto node = _head;
		if (node != nullptr)
		{
			_head = node->_next;
			if (_head == nullptr)
				_tail = nullptr;
		}
		return node;
	}
	[[nodiscard]] constexpr bool unlink(node_type* target) noexcept
	{
		auto link = &_head;
		auto previous = static_cast<node_type*>(nullptr);
		while (*link != nullptr)
		{
			if (*link == target)
			{
				*link = target->_next;
				if (_tail == target)
					_tail = previous;
				return true;
			}
			previous = *link;
			link = &(*link)->_next;
		}
		return false;
	}
};

NAGISA_BUILD_LIB_DETAIL_END
