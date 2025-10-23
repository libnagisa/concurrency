#pragma once

#include "./_coroutine/awaiter.h"
#include "./_coroutine/awaitable.h"
#include "./_coroutine/promise.h"
#include "./_coroutine/awaitable_trait.h"
#include "./_coroutine/task.h"

#include "./_coroutine/component/intro.h"
#include "./_coroutine/component/exception.h"
#include "./_coroutine/component/value.h"
#include "./_coroutine/component/return_object.h"
#include "./_coroutine/component/schedulable.h"
#include "./_coroutine/component/workflow.h"
#include "./_coroutine/component/with_awaitable.h"
#include "./environment.h"

NAGISA_BUILD_LIB_BEGIN

using details::awaitable;
using details::awaitable_awaiter_t;
using details::awaiter;
using details::await_suspend_result;
using details::awaiter_result_t;

using details::awaitable_trait_combiner;
using details::awaitable_trait_combiner_t;
using details::awaitable_trait_instance;
using details::awaitable_trait_instance_t;

using details::basic_task;
using details::make_basic_task_with_trait;
using details::make_basic_task_with_trait_t;

namespace promises = details::promises;
namespace awaitable_traits = details::awaitable_traits;

using details::get_unhandled_exception_ptr;
using details::get_unhandled_exception_ptr_t;
using details::release_returned_value;
using details::release_returned_value_t;
using details::set_continuation;
using details::set_continuation_t;
using details::continuation;
using details::continuation_t;



NAGISA_BUILD_LIB_END

#include <nagisa/build_lib/destruct.h>