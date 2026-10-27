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

function(nagisa_concurrency_resource_pool_test name)
	add_executable(resource_pool_${name} ${name}.cpp)
	target_link_libraries(resource_pool_${name} PRIVATE nagisa::concurrency doctest::doctest Threads::Threads)
	target_include_directories(resource_pool_${name} PRIVATE
		"${STDEXEC_INCLUDE_DIR}"
		"${CMAKE_CURRENT_SOURCE_DIR}"
	)
	target_compile_features(resource_pool_${name} PRIVATE cxx_std_20)
	set_target_properties(resource_pool_${name} PROPERTIES
		CXX_STANDARD 23
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
	)
	if(MSVC)
		target_compile_options(resource_pool_${name} PRIVATE /W3 /Zc:preprocessor /Zc:__cplusplus /utf-8 /wd4100)
	endif()
	add_test(NAME resource_pool.${name} COMMAND resource_pool_${name})
endfunction()

nagisa_concurrency_resource_pool_test(token_concept)
nagisa_concurrency_resource_pool_test(empty)
nagisa_concurrency_resource_pool_test(construct_copy)
nagisa_concurrency_resource_pool_test(construct_move)
nagisa_concurrency_resource_pool_test(construct_in_place)
nagisa_concurrency_resource_pool_test(try_acquire)
nagisa_concurrency_resource_pool_test(lifo)
nagisa_concurrency_resource_pool_test(raii)
nagisa_concurrency_resource_pool_test(exception_empty)
nagisa_concurrency_resource_pool_test(exception_back)
nagisa_concurrency_resource_pool_test(exception_pop_back)
nagisa_concurrency_resource_pool_test(move_only)
nagisa_concurrency_resource_pool_test(move_only_unique_ptr)
nagisa_concurrency_resource_pool_test(move_only_acquire)
nagisa_concurrency_resource_pool_test(move_only_bounded)
nagisa_concurrency_resource_pool_test(bounded)
nagisa_concurrency_resource_pool_test(custom_token)
nagisa_concurrency_resource_pool_test(acquire_sync_wait)
nagisa_concurrency_resource_pool_test(then)
nagisa_concurrency_resource_pool_test(let_value)
nagisa_concurrency_resource_pool_test(co_await)
nagisa_concurrency_resource_pool_test(co_await_wait)

nagisa_concurrency_resource_pool_test(handoff)
nagisa_concurrency_resource_pool_test(cancel_waiting)
nagisa_concurrency_resource_pool_test(concurrent_cancel_stress)
nagisa_concurrency_resource_pool_test(concurrency_stress)
nagisa_concurrency_resource_pool_test(error_signature)
nagisa_concurrency_resource_pool_test(single_slot)
