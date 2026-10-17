file(GLOB PROFILE_FILES "${PROFILE_DIR}/kafka-*.profraw")
if(NOT PROFILE_FILES)
	message(FATAL_ERROR "No LLVM profile data was produced by the unit tests")
endif()

set(PROFILE_DATA "${PROFILE_DIR}/coverage.profdata")
execute_process(
	COMMAND "${LLVM_PROFDATA}" merge -sparse ${PROFILE_FILES} -o "${PROFILE_DATA}"
	RESULT_VARIABLE MERGE_RESULT
)
if(NOT MERGE_RESULT EQUAL 0)
	message(FATAL_ERROR "llvm-profdata failed to merge the unit-test profiles")
endif()

if(NOT DEFINED SOURCE_FILES OR "${SOURCE_FILES}" STREQUAL "")
	message(FATAL_ERROR "No *_test.cpp source files were provided to coverage generation")
endif()

message(STATUS "Coverage report for:")
foreach(SOURCE_FILE IN LISTS SOURCE_FILES)
	message(STATUS "  ${SOURCE_FILE}")
endforeach()

execute_process(
	COMMAND "${LLVM_COV}" report "${TEST_EXECUTABLE}" "-instr-profile=${PROFILE_DATA}" ${SOURCE_FILES}
	RESULT_VARIABLE REPORT_RESULT
)
if(NOT REPORT_RESULT EQUAL 0)
	message(FATAL_ERROR "llvm-cov failed to generate the coverage summary")
endif()

execute_process(
	COMMAND "${LLVM_COV}" show "${TEST_EXECUTABLE}" "-instr-profile=${PROFILE_DATA}" ${SOURCE_FILES}
		-format=html "-output-dir=${HTML_DIR}"
	RESULT_VARIABLE HTML_RESULT
)
if(NOT HTML_RESULT EQUAL 0)
	message(FATAL_ERROR "llvm-cov failed to generate the HTML coverage report")
endif()

message(STATUS "HTML coverage report: ${HTML_DIR}/index.html")