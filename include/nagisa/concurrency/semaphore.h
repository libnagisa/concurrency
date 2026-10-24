#pragma once

#include "./_semaphore/wait_queue.h"
#include "./_semaphore/basic_semaphore.h"
#include "./_semaphore/counting_semaphore.h"
#include "./_semaphore/binary_semaphore.h"


#include "./environment.h"

NAGISA_BUILD_LIB_BEGIN

using details::wait_queue;
using details::basic_semaphore;
using details::counting_semaphore;
using details::binary_semaphore;

NAGISA_BUILD_LIB_END

#include <nagisa/build_lib/destruct.h>
