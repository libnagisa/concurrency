find_package(Threads REQUIRED)

find_path(STDEXEC_INCLUDE_DIR
	NAMES stdexec/execution.hpp
	HINTS
		"$ENV{NAGISA_LIBRARY_ROOT}/stdexec/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/../../../../../stdexec/include"
)
if(NOT STDEXEC_INCLUDE_DIR)
	message(FATAL_ERROR "stdexec/execution.hpp not found; set NAGISA_LIBRARY_ROOT or add stdexec to the include path")
endif()

function(nagisa_concurrency_lease_test name)
	add_executable(lease_${name} ${name}.cpp)
	target_link_libraries(lease_${name} PRIVATE nagisa::concurrency doctest::doctest Threads::Threads)
	target_include_directories(lease_${name} PRIVATE
		"${STDEXEC_INCLUDE_DIR}"
		"${CMAKE_CURRENT_SOURCE_DIR}"
	)
	target_compile_features(lease_${name} PRIVATE cxx_std_20)
	set_target_properties(lease_${name} PROPERTIES
		CXX_STANDARD 23
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
	)
	if(MSVC)
		target_compile_options(lease_${name} PRIVATE /W3 /Zc:preprocessor /Zc:__cplusplus /utf-8 /wd4100)
	endif()
	add_test(NAME lease.${name} COMMAND lease_${name})
endfunction()

nagisa_concurrency_lease_test(token_concept)
nagisa_concurrency_lease_test(empty)
nagisa_concurrency_lease_test(construct_copy)
nagisa_concurrency_lease_test(construct_move)
nagisa_concurrency_lease_test(construct_in_place)
nagisa_concurrency_lease_test(try_acquire)
nagisa_concurrency_lease_test(lifo)
nagisa_concurrency_lease_test(raii)
nagisa_concurrency_lease_test(exception_empty)
nagisa_concurrency_lease_test(exception_back)
nagisa_concurrency_lease_test(exception_pop_back)
nagisa_concurrency_lease_test(exception_push_back)
nagisa_concurrency_lease_test(exception_size)
nagisa_concurrency_lease_test(move_only)
nagisa_concurrency_lease_test(move_only_unique_ptr)
nagisa_concurrency_lease_test(move_only_acquire)
nagisa_concurrency_lease_test(move_only_bounded)
nagisa_concurrency_lease_test(supply)
nagisa_concurrency_lease_test(bounded)
nagisa_concurrency_lease_test(custom_token)
nagisa_concurrency_lease_test(acquire_sync_wait)
nagisa_concurrency_lease_test(then)
nagisa_concurrency_lease_test(let_value)
nagisa_concurrency_lease_test(co_await)
nagisa_concurrency_lease_test(fifo_waiters)
nagisa_concurrency_lease_test(supply_wakes)
nagisa_concurrency_lease_test(release_wakes)
nagisa_concurrency_lease_test(cancel_waiting)
nagisa_concurrency_lease_test(cancel_already_stopped)
nagisa_concurrency_lease_test(stop_ignored)
nagisa_concurrency_lease_test(when_all_cancel)
nagisa_concurrency_lease_test(co_await_wait)
nagisa_concurrency_lease_test(limiter)
