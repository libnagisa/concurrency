#pragma once

#include <cstddef>
#include <vector>

inline ::std::size_t heavy_cpu(::std::size_t N)
{
    std::vector sieve(N + 1, 1);
    sieve[0] = sieve[1] = 0;

    ::std::size_t result_hash = 0;

    for (::std::size_t i = 2; i * i <= N; ++i) {
        if (sieve[i]) {
            for (::std::size_t j = i * i; j <= N; j += i) {
                sieve[j] = 0;
            }
        }
    }

    for (::std::size_t i = 2; i <= N; ++i) {
        if (sieve[i])
            result_hash = (result_hash * 1315423911) ^ i;
    }

    return result_hash;
}