cmake_minimum_required(VERSION 3.15)
project(ConanHelloWorld CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_COVERAGE "Instrument unit tests and add a coverage target" OFF)

find_package(fmt CONFIG REQUIRED)
find_package(quill CONFIG REQUIRED)
find_package(RdKafka CONFIG REQUIRED)

add_executable(hello_world cmd/helloworld/main.cpp)
add_executable(kafka_consumer cmd/domain1/consumer.cpp)
add_executable(multithread cmd/multithread/multithread.cpp)
add_executable(circle_checker cmd/circle-checker/circle-checker.cpp)
#  FORCE TARGET STATIC LINKING
# This tells the compiler to bundle your source code and fmt together 
# without breaking Android's core dynamic system dependencies.
target_link_libraries(hello_world PRIVATE fmt::fmt quill::quill RdKafka::rdkafka++)
target_link_libraries(kafka_consumer PRIVATE RdKafka::rdkafka++)
target_link_libraries(multithread PRIVATE fmt::fmt quill::quill RdKafka::rdkafka++)
target_link_libraries(circle_checker PRIVATE fmt::fmt quill::quill RdKafka::rdkafka++)

include(CTest)
if(BUILD_TESTING)
	find_package(GTest CONFIG REQUIRED)

	file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/*_test.cpp")
	list(FILTER UNIT_TEST_SOURCES EXCLUDE REGEX "/build/")
	if(NOT UNIT_TEST_SOURCES)
		message(FATAL_ERROR "No *_test.cpp files were found for unit test discovery")
	endif()

	add_executable(unit_tests ${UNIT_TEST_SOURCES})
	target_link_libraries(unit_tests PRIVATE
		GTest::gmock_main
		GTest::gtest_main
		RdKafka::rdkafka++
	)

	include(GoogleTest)
	gtest_discover_tests(unit_tests)

	if(ENABLE_COVERAGE)
		if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			message(FATAL_ERROR "ENABLE_COVERAGE requires Clang or AppleClang")
		endif()

		if(APPLE)
			find_program(XCRUN_EXECUTABLE xcrun)
			if(NOT XCRUN_EXECUTABLE)
				message(FATAL_ERROR "xcrun is required to locate Apple's LLVM coverage tools")
			endif()
			execute_process(COMMAND "${XCRUN_EXECUTABLE}" --find llvm-profdata
				OUTPUT_VARIABLE LLVM_PROFDATA_EXECUTABLE
				OUTPUT_STRIP_TRAILING_WHITESPACE
				RESULT_VARIABLE LLVM_PROFDATA_RESULT)
			execute_process(COMMAND "${XCRUN_EXECUTABLE}" --find llvm-cov
				OUTPUT_VARIABLE LLVM_COV_EXECUTABLE
				OUTPUT_STRIP_TRAILING_WHITESPACE
				RESULT_VARIABLE LLVM_COV_RESULT)
			if(NOT LLVM_PROFDATA_RESULT EQUAL 0 OR NOT LLVM_COV_RESULT EQUAL 0)
				message(FATAL_ERROR "Could not locate llvm-profdata and llvm-cov using xcrun")
			endif()
		else()
			find_program(LLVM_PROFDATA_EXECUTABLE llvm-profdata)
			find_program(LLVM_COV_EXECUTABLE llvm-cov)
			if(NOT LLVM_PROFDATA_EXECUTABLE OR NOT LLVM_COV_EXECUTABLE)
				message(FATAL_ERROR "llvm-profdata and llvm-cov are required for coverage")
			endif()
		endif()

		target_compile_options(unit_tests PRIVATE
			-fprofile-instr-generate
			-fcoverage-mapping
		)
		target_link_options(unit_tests PRIVATE -fprofile-instr-generate)

		file(GLOB_RECURSE ALL_CPP_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/**/*.cpp")
		file(GLOB_RECURSE ALL_HEADER_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/**/*.[hH]pp")
		set(COVERAGE_SOURCE_FILES ${ALL_CPP_SOURCES} ${ALL_HEADER_SOURCES})
		list(FILTER COVERAGE_SOURCE_FILES EXCLUDE REGEX "_test\\.(cpp|hpp)$")
		list(FILTER COVERAGE_SOURCE_FILES EXCLUDE REGEX "/build/")
		list(REMOVE_DUPLICATES COVERAGE_SOURCE_FILES)
		if(NOT COVERAGE_SOURCE_FILES)
			message(FATAL_ERROR "No production C++ or header files were found for coverage reporting")
		endif()

		set(COVERAGE_DATA_DIR "${CMAKE_BINARY_DIR}/coverage-data")
		set(COVERAGE_HTML_DIR "${CMAKE_BINARY_DIR}/coverage-html")
		add_custom_target(coverage
			COMMAND "${CMAKE_COMMAND}" -E rm -rf "${COVERAGE_DATA_DIR}" "${COVERAGE_HTML_DIR}"
			COMMAND "${CMAKE_COMMAND}" -E make_directory "${COVERAGE_DATA_DIR}"
			COMMAND "${CMAKE_COMMAND}" -E env
				"LLVM_PROFILE_FILE=${COVERAGE_DATA_DIR}/unit_tests-%p.profraw"
				"${CMAKE_COMMAND}" -E chdir "${CMAKE_BINARY_DIR}"
				"${CMAKE_CTEST_COMMAND}" --output-on-failure
			COMMAND "${CMAKE_COMMAND}"
				"-DLLVM_PROFDATA=${LLVM_PROFDATA_EXECUTABLE}"
				"-DLLVM_COV=${LLVM_COV_EXECUTABLE}"
				"-DTEST_EXECUTABLE=$<TARGET_FILE:unit_tests>"
				"-DPROFILE_DIR=${COVERAGE_DATA_DIR}"
				"-DSOURCE_FILES=${COVERAGE_SOURCE_FILES}"
				"-DHTML_DIR=${COVERAGE_HTML_DIR}"
				-P "${PROJECT_SOURCE_DIR}/cmake/GenerateCoverage.cmake"
			DEPENDS unit_tests
			USES_TERMINAL
			VERBATIM
		)
	endif()
endif()
