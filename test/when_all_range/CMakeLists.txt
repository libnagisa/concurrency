find_package(Threads REQUIRED)
find_path(STDEXEC_INCLUDE_DIR
	NAMES stdexec/execution.hpp
	HINTS
		"$ENV{NAGISA_LIBRARY_ROOT}/stdexec/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/../../../../../stdexec/include"
)
if(NOT STDEXEC_INCLUDE_DIR)
	message(FATAL_ERROR "stdexec/execution.hpp not found; set STDEXEC_INCLUDE_DIR")
endif()

function(nagisa_concurrency_when_all_range_test name)
	add_executable(when_all_range_${name} ${name}.cpp)
	target_link_libraries(when_all_range_${name} PRIVATE nagisa::concurrency doctest::doctest Threads::Threads)
	target_include_directories(when_all_range_${name} PRIVATE "${STDEXEC_INCLUDE_DIR}")
	set_target_properties(when_all_range_${name} PROPERTIES
		CXX_STANDARD 23
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
	)
	if(MSVC)
		target_compile_options(when_all_range_${name} PRIVATE /W3 /Zc:preprocessor /Zc:__cplusplus /utf-8 /wd4100)
	endif()
	add_test(NAME when_all_range.${name} COMMAND when_all_range_${name})
	set_tests_properties(when_all_range.${name} PROPERTIES TIMEOUT 30)
endfunction()

nagisa_concurrency_when_all_range_test(basic)
nagisa_concurrency_when_all_range_test(completion)
nagisa_concurrency_when_all_range_test(cancellation)
nagisa_concurrency_when_all_range_test(lifetime)
nagisa_concurrency_when_all_range_test(environment)
nagisa_concurrency_when_all_range_test(concurrent)
nagisa_concurrency_when_all_range_test(co_await)
