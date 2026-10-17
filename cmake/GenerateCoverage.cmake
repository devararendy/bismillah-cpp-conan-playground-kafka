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

message(STATUS "Coverage report for kafka.hpp:")
execute_process(
	COMMAND "${LLVM_COV}" report "${TEST_EXECUTABLE}" "-instr-profile=${PROFILE_DATA}" "${SOURCE_FILE}"
	RESULT_VARIABLE REPORT_RESULT
)
if(NOT REPORT_RESULT EQUAL 0)
	message(FATAL_ERROR "llvm-cov failed to generate the coverage summary")
endif()

execute_process(
	COMMAND "${LLVM_COV}" show "${TEST_EXECUTABLE}" "-instr-profile=${PROFILE_DATA}" "${SOURCE_FILE}"
		-format=html "-output-dir=${HTML_DIR}"
	RESULT_VARIABLE HTML_RESULT
)
if(NOT HTML_RESULT EQUAL 0)
	message(FATAL_ERROR "llvm-cov failed to generate the HTML coverage report")
endif()

message(STATUS "HTML coverage report: ${HTML_DIR}/index.html")