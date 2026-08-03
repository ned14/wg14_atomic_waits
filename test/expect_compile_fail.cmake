# Driver for a compile-fail test. Builds TARGET_NAME, which must FAIL to compile,
# and asserts that the compiler emitted a diagnostic containing DIAGNOSTIC as a
# literal substring (not a regex). The build output is echoed so it is visible in
# the test log. If the build succeeds or the expected diagnostic text is absent,
# the script exits non-zero so the test fails.
#
# DIAGNOSTIC is our own static_assert message text, which every supported compiler
# (gcc, clang, MSVC) echoes verbatim for static_assert diagnostics. Matching it
# literally avoids two brittleness sources of the old regex approach:
#   - no regex metacharacters in the message need escaping, so CMake/CTest plumbing
#     and the MSVC backslash handling cannot corrupt the pattern;
#   - the expected text is namespaced ("wg14_atomic_waits: ..."), so a compile
#     failure for an unrelated reason cannot be mistaken for the intended one.
#
# Usage: cmake -DBINARY_DIR=.. -DTARGET_NAME=.. -DDIAGNOSTIC=<expected text>
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

# The target must fail to compile ...
if(result EQUAL 0)
  message(FATAL_ERROR
          "${TARGET_NAME} compiled successfully but must fail to compile")
endif()

# ... and must fail for the intended reason: the diagnostic must contain our
# static_assert message text (matched literally, so compiler quoting style cannot
# cause a false negative and unrelated diagnostics cannot cause a false positive).
string(FIND "${all_output}" "${DIAGNOSTIC}" diag_pos)
if(diag_pos EQUAL -1)
  message(FATAL_ERROR
          "${TARGET_NAME} did not emit the expected diagnostic text "
          "'${DIAGNOSTIC}'")
endif()
