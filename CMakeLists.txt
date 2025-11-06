cmake_minimum_required(VERSION 3.5...3.20)

# project(nagisa_concurrency VERSION "${NAGISA_SUPERPROJECT_VERSION}" LANGUAGE CXX)
project(nagisa_concurrency LANGUAGES CXX)

add_library(nagisa_concurrency INTERFACE)

add_library(nagisa::concurrency ALIAS nagisa_concurrency)

target_include_directories(nagisa_concurrency
	INTERFACE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
)
target_link_libraries(nagisa_concurrency INTERFACE nagisa::build_lib)
target_compile_features(nagisa_concurrency INTERFACE cxx_std_20)

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
	add_subdirectory(test)
endif()

include(GNUInstallDirs)
install(
	TARGETS nagisa_concurrency
	EXPORT  nagisa_concurrency_targets
)
install(
	DIRECTORY include/
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
	EXPORT nagisa_concurrency_targets
	NAMESPACE nagisa::
	FILE nagisa_concurrencyConfig.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nagisa_concurrency
)