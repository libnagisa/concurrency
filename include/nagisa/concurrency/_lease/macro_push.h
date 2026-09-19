// #pragma once

// see https://developercommunity.visualstudio.com/t/MSVC-wd4100-instantiation-ICE/11115905
#pragma push_macro("NAGISA_CONCURRENCY_LEASE_ASSERT")

#ifdef __cpp_contracts
#	define NAGISA_CONCURRENCY_LEASE_ASSERT(condition) contract_assert(condition)
#else
#	define NAGISA_CONCURRENCY_LEASE_ASSERT(condition) assert(condition)
#endif