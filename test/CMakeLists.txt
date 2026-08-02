add_code_test(atomic_wait_test SOURCES "atomic_wait_test.c" FEATURES c_std_11)
add_code_test(atomic_wait_expected_test SOURCES
              "atomic_wait_expected_test.c" FEATURES c_std_11)
add_code_test(atomic_notify_test SOURCES
              "atomic_notify_test.c" FEATURES c_std_11)
add_code_test(benchmark_atomic_wait_test SOURCES
              "benchmark_atomic_wait_test.c" FEATURES c_std_11)

add_executable(header_only_test "header_only_test.cpp"
                                "header_only_test1.cpp"
                                "header_only_test2.cpp")
target_include_directories(header_only_test PRIVATE
                           "${CMAKE_CURRENT_SOURCE_DIR}/../include")
if(MSVC)
  target_compile_options(header_only_test PRIVATE /W4)
else()
  target_compile_options(header_only_test PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_compile_features(header_only_test PRIVATE c_std_11 cxx_std_20)
target_link_libraries(header_only_test PRIVATE ${PROJECT_NAME})
set_target_properties(header_only_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                      "${CMAKE_BINARY_DIR}/bin")
add_test(NAME header_only_test COMMAND $<TARGET_FILE:header_only_test>)

# Compile-fail tests: each source deliberately uses an invalid input type for a
# macro in atomic_wait.h and must FAIL to compile, with the compiler emitting a
# specific diagnostic (DIAGNOSTIC is the regex to expect). Both C (.c) and C++
# (.cpp) variants are checked. EXCLUDE_FROM_ALL keeps the targets out of the
# default build. Each test runs expect_compile_fail.cmake, which echoes the build
# output (so the diagnostic is visible) and exits non-zero if the expected
# diagnostic was not emitted. The test passes via PASS_REGULAR_EXPRESSION if and
# only if that diagnostic appears; when it is absent the non-zero exit fails it.
function(add_compile_fail_test name source diagnostic)
  add_executable(${name} EXCLUDE_FROM_ALL "${source}")
  target_link_libraries(${name} PRIVATE ${PROJECT_NAME})
  if(MSVC)
    target_compile_options(${name} PRIVATE /W4)
  else()
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  if(source MATCHES "\\.c$")
    target_compile_features(${name} PRIVATE c_std_11)
  else()
    target_compile_features(${name} PRIVATE cxx_std_11)
  endif()
  set_target_properties(${name} PROPERTIES EXCLUDE_FROM_ALL TRUE)
  add_test(
    NAME ${name}
    COMMAND ${CMAKE_COMMAND}
      -DTARGET_NAME=${name}
      -DBINARY_DIR=${CMAKE_BINARY_DIR}
      -DDIAGNOSTIC=${diagnostic}
      -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_compile_fail.cmake"
  )
  set_tests_properties(${name} PROPERTIES PASS_REGULAR_EXPRESSION "${diagnostic}")
endfunction()

set(COMPILE_FAIL_DIR "compile_fail")
set(WIDTH_DIAGNOSTIC "must be 1, 2, 4, or 8 bytes")
set(NATIVE_DIAGNOSTIC_C "must equal sizeof")
set(NATIVE_DIAGNOSTIC_CPP "size is negative")
add_compile_fail_test(compile_fail_atomic_wait_c
                      "${COMPILE_FAIL_DIR}/atomic_wait_badtype.c"
                      "${WIDTH_DIAGNOSTIC}")
add_compile_fail_test(compile_fail_atomic_wait_explicit_c
                      "${COMPILE_FAIL_DIR}/atomic_wait_explicit_badtype.c"
                      "${WIDTH_DIAGNOSTIC}")
add_compile_fail_test(compile_fail_atomic_notify_one_c
                      "${COMPILE_FAIL_DIR}/atomic_notify_one_badtype.c"
                      "${WIDTH_DIAGNOSTIC}")
add_compile_fail_test(compile_fail_atomic_notify_all_c
                      "${COMPILE_FAIL_DIR}/atomic_notify_all_badtype.c"
                      "${WIDTH_DIAGNOSTIC}")
add_compile_fail_test(compile_fail_atomic_wait_expected_c
                      "${COMPILE_FAIL_DIR}/atomic_wait_expected_badtype.c"
                      "${NATIVE_DIAGNOSTIC_C}")
add_compile_fail_test(compile_fail_atomic_notify_c
                      "${COMPILE_FAIL_DIR}/atomic_notify_badtype.c"
                      "${NATIVE_DIAGNOSTIC_C}")
add_compile_fail_test(compile_fail_atomic_wait_cpp
                      "${COMPILE_FAIL_DIR}/atomic_wait_badtype.cpp"
                      "${WIDTH_DIAGNOSTIC}")
add_compile_fail_test(compile_fail_atomic_wait_explicit_cpp
                      "${COMPILE_FAIL_DIR}/atomic_wait_explicit_badtype.cpp"
                      "${WIDTH_DIAGNOSTIC}")
add_compile_fail_test(compile_fail_atomic_notify_one_cpp
                      "${COMPILE_FAIL_DIR}/atomic_notify_one_badtype.cpp"
                      "${WIDTH_DIAGNOSTIC}")
add_compile_fail_test(compile_fail_atomic_notify_all_cpp
                      "${COMPILE_FAIL_DIR}/atomic_notify_all_badtype.cpp"
                      "${WIDTH_DIAGNOSTIC}")
add_compile_fail_test(compile_fail_atomic_wait_expected_cpp
                      "${COMPILE_FAIL_DIR}/atomic_wait_expected_badtype.cpp"
                      "${NATIVE_DIAGNOSTIC_CPP}")
add_compile_fail_test(compile_fail_atomic_notify_cpp
                      "${COMPILE_FAIL_DIR}/atomic_notify_badtype.cpp"
                      "${NATIVE_DIAGNOSTIC_CPP}")
