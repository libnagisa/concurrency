#pragma once

#include "./_lease/token.h"
#include "./_lease/bounded_lease_pool.h"
#include "./_lease/lease_pool.h"


#include "./environment.h"

NAGISA_BUILD_LIB_BEGIN

using details::lease_token;
using details::lease_pool;
using details::bounded_lease_pool;

NAGISA_BUILD_LIB_END

#include <nagisa/build_lib/destruct.h>
