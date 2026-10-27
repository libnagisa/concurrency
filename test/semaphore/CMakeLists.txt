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

function(nagisa_concurrency_semaphore_test name)
	add_executable(semaphore_${name} ${name}.cpp)
	target_link_libraries(semaphore_${name} PRIVATE nagisa::concurrency doctest::doctest Threads::Threads)
	target_include_directories(semaphore_${name} PRIVATE
		"${STDEXEC_INCLUDE_DIR}"
		"${CMAKE_CURRENT_SOURCE_DIR}"
	)
	target_compile_features(semaphore_${name} PRIVATE cxx_std_20)
	set_target_properties(semaphore_${name} PROPERTIES
		CXX_STANDARD 23
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
	)
	if(MSVC)
		target_compile_options(semaphore_${name} PRIVATE /W3 /Zc:preprocessor /Zc:__cplusplus /utf-8 /wd4100)
	endif()
	add_test(NAME semaphore.${name} COMMAND semaphore_${name})
endfunction()

nagisa_concurrency_semaphore_test(try_acquire)
nagisa_concurrency_semaphore_test(fifo_wakeup)
nagisa_concurrency_semaphore_test(cancel_waiting)
nagisa_concurrency_semaphore_test(cancel_already_stopped)
nagisa_concurrency_semaphore_test(cancel_already_stopped_with_permit)
nagisa_concurrency_semaphore_test(binary)
nagisa_concurrency_semaphore_test(concurrency_stress)
nagisa_concurrency_semaphore_test(concurrent_cancel_stress)
nagisa_concurrency_semaphore_test(release_many)
nagisa_concurrency_semaphore_test(least_max)
nagisa_concurrency_semaphore_test(release_many_fast)
nagisa_concurrency_semaphore_test(release_many_cancel)
