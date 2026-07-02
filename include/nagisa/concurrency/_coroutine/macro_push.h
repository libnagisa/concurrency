// #pragma once

// see https://developercommunity.visualstudio.com/t/MSVC-wd4100-instantiation-ICE/11115905
#pragma push_macro("NAGISA_CONCURRENCY_WD4100_INSTANTIATION_ICE")
#if defined(_MSC_VER) && !defined(__clang__)
#	define NAGISA_CONCURRENCY_WD4100_INSTANTIATION_ICE(type, or_) type
#else
#	define NAGISA_CONCURRENCY_WD4100_INSTANTIATION_ICE(type, or_) or_
#endif