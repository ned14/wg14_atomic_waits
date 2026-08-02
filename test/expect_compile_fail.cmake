# Driver for a compile-fail test. Builds TARGET_NAME, which must FAIL to compile,
# and asserts that the compiler emits a diagnostic matching DIAGNOSTIC (a regex).
# The build output is echoed so that the diagnostic is visible to the test's
# PASS_REGULAR_EXPRESSION property. If the expected diagnostic is absent, the
# script exits non-zero so the test fails.
#
# Usage: cmake -DBINARY_DIR=.. -DTARGET_NAME=.. -DDIAGNOSTIC=<regex>
#              -P expect_compile_fail.cmake
if(NOT DEFINED TARGET_NAME OR NOT DEFINED BINARY_DIR OR NOT DEFINED DIAGNOSTIC)
  message(FATAL_ERROR
          "Usage: -DTARGET_NAME=.. -DBINARY_DIR=.. -DDIAGNOSTIC=.. -P "
          "expect_compile_fail.cmake")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${BINARY_DIR}" --target "${TARGET_NAME}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)

set(all_output "${output}\n${error}")
message("${all_output}")

if(NOT all_output MATCHES "${DIAGNOSTIC}")
  message(FATAL_ERROR "${TARGET_NAME} did not emit a diagnostic matching "
                      "'${DIAGNOSTIC}'")
endif()
