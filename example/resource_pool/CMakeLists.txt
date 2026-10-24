find_path(STDEXEC_INCLUDE_DIR
	NAMES stdexec/execution.hpp
	HINTS
		"$ENV{NAGISA_LIBRARY_ROOT}/stdexec/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/../../../../../stdexec/include"
)
if(NOT STDEXEC_INCLUDE_DIR)
	message(FATAL_ERROR "stdexec/execution.hpp not found; set NAGISA_LIBRARY_ROOT or add stdexec to the include path")
endif()

function(nagisa_concurrency_resource_pool_example name)
	add_executable(example_resource_pool_${name} ${name}.cpp)
	target_link_libraries(example_resource_pool_${name} PRIVATE nagisa::concurrency)
	target_include_directories(example_resource_pool_${name} PRIVATE "${STDEXEC_INCLUDE_DIR}")
	target_compile_features(example_resource_pool_${name} PRIVATE cxx_std_20)
	set_target_properties(example_resource_pool_${name} PROPERTIES
		CXX_STANDARD 23
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
	)
	if(MSVC)
		target_compile_options(example_resource_pool_${name} PRIVATE /W3 /Zc:preprocessor /Zc:__cplusplus /utf-8 /wd4100)
	endif()
	add_test(NAME example.resource_pool.${name} COMMAND example_resource_pool_${name})
endfunction()

nagisa_concurrency_resource_pool_example(resource_pool)
nagisa_concurrency_resource_pool_example(limiter)
nagisa_concurrency_resource_pool_example(acquire)
nagisa_concurrency_resource_pool_example(move_only)
nagisa_concurrency_resource_pool_example(exception)
nagisa_concurrency_resource_pool_example(release_wakes)
