cmake_minimum_required(VERSION 3.15)
project(ConanHelloWorld CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(fmt CONFIG REQUIRED)
find_package(quill CONFIG REQUIRED)
find_package(RdKafka CONFIG REQUIRED)

add_executable(hello_world cmd/helloworld/main.cpp)
add_executable(kafka_consumer cmd/domain1/consumer.cpp)

#  FORCE TARGET STATIC LINKING
# This tells the compiler to bundle your source code and fmt together 
# without breaking Android's core dynamic system dependencies.
target_link_libraries(hello_world PRIVATE fmt::fmt quill::quill RdKafka::rdkafka++)
target_link_libraries(kafka_consumer PRIVATE RdKafka::rdkafka++)

include(CTest)
if(BUILD_TESTING)
	find_package(GTest CONFIG REQUIRED)

	add_executable(kafka_unit_tests
		internal/common/kafka/kafka_test.cpp
	)
	target_link_libraries(kafka_unit_tests PRIVATE
		GTest::gtest_main
		RdKafka::rdkafka++
	)

	include(GoogleTest)
	gtest_discover_tests(kafka_unit_tests)
endif()
