find_path(STDEXEC_INCLUDE_DIR
	NAMES stdexec/execution.hpp
	HINTS
		"$ENV{NAGISA_LIBRARY_ROOT}/stdexec/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/../../../../../stdexec/include"
)
if(NOT STDEXEC_INCLUDE_DIR)
	message(FATAL_ERROR "stdexec/execution.hpp not found; set NAGISA_LIBRARY_ROOT or add stdexec to the include path")
endif()

function(nagisa_concurrency_lease_example name)
	add_executable(example_lease_${name} ${name}.cpp)
	target_link_libraries(example_lease_${name} PRIVATE nagisa::concurrency)
	target_include_directories(example_lease_${name} PRIVATE "${STDEXEC_INCLUDE_DIR}")
	target_compile_features(example_lease_${name} PRIVATE cxx_std_20)
	set_target_properties(example_lease_${name} PROPERTIES
		CXX_STANDARD 23
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
	)
	if(MSVC)
		target_compile_options(example_lease_${name} PRIVATE /W3 /Zc:preprocessor /Zc:__cplusplus /utf-8 /wd4100)
	endif()
	add_test(NAME example.lease.${name} COMMAND example_lease_${name})
endfunction()

nagisa_concurrency_lease_example(resource_pool)
nagisa_concurrency_lease_example(limiter)
nagisa_concurrency_lease_example(acquire)
nagisa_concurrency_lease_example(move_only)
nagisa_concurrency_lease_example(exception)
nagisa_concurrency_lease_example(supply)
