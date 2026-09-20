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

function(nagisa_concurrency_when_all_example name)
	add_executable(example_when_all_${name} ${name}.cpp)
	target_link_libraries(example_when_all_${name} PRIVATE nagisa::concurrency Threads::Threads)
	target_include_directories(example_when_all_${name} PRIVATE "${STDEXEC_INCLUDE_DIR}")
	set_target_properties(example_when_all_${name} PROPERTIES
		CXX_STANDARD 23
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
	)
	if(MSVC)
		target_compile_options(example_when_all_${name} PRIVATE /W3 /Zc:preprocessor /Zc:__cplusplus /utf-8 /wd4100)
	endif()
	if(BUILD_TESTING)
		add_test(NAME example.when_all.${name} COMMAND example_when_all_${name})
		set_tests_properties(example.when_all.${name} PROPERTIES TIMEOUT 30)
	endif()
endfunction()

nagisa_concurrency_when_all_example(range)
nagisa_concurrency_when_all_example(parallel)
nagisa_concurrency_when_all_example(co_await)
nagisa_concurrency_when_all_example(error)
