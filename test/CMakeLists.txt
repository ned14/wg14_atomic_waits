add_code_test(atomic_wait_test SOURCES "atomic_wait_test.c" FEATURES c_std_11)
add_code_test(atomic_wait_expected_test SOURCES
              "atomic_wait_expected_test.c" FEATURES c_std_11)
add_code_test(atomic_notify_test SOURCES
              "atomic_notify_test.c" FEATURES c_std_11)
add_code_test(atomic_wait_more_test SOURCES
              "atomic_wait_more_test.c" FEATURES c_std_11)
add_code_test(atomic_wait_widths_test SOURCES
              "atomic_wait_widths_test.c" FEATURES c_std_11)
add_code_test(atomic_notify_more_test SOURCES
              "atomic_notify_more_test.c" FEATURES c_std_11)
add_code_test(atomic_wait_race_test SOURCES
              "atomic_wait_race_test.c" FEATURES c_std_11)
add_code_test(wait_expected_leak_test SOURCES
              "wait_expected_leak_test.c" FEATURES c_std_11)
add_code_test(hash_table_whitebox_test SOURCES
              "hash_table_whitebox_test.c" FEATURES c_std_11)
# POSIX-only white-box test of the pthreads backend's wake-token accounting
# (the included backend .ipp has no meaning on Windows, which lacks
# <pthread.h>). Threads::Threads supplies -pthread on Linux; macOS/FreeBSD
# provide pthreads in the system C library.
if(NOT WIN32)
  add_code_test(pthreads_proxy_whitebox_test SOURCES
                "pthreads_proxy_whitebox_test.c" FEATURES c_std_11)
  find_package(Threads REQUIRED)
  target_link_libraries(pthreads_proxy_whitebox_test PRIVATE Threads::Threads)
endif()

# Benchmark: built only on demand (EXCLUDE_FROM_ALL) and excluded from ctest
# with -E benchmark so ordinary test runs stay fast and deterministic.
add_code_example(benchmark_atomic_wait_test SOURCES
                 "benchmark_atomic_wait_test.c" FEATURES c_std_11
                 PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_test(NAME benchmark_atomic_wait_test
         COMMAND $<TARGET_FILE:benchmark_atomic_wait_test>)

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

# Install + find_package consumer test: stages `cmake --install` into a scratch
# prefix, checks the headers / package config / version file landed, then
# configures, builds and runs the standalone consumer in install_consumer/
# against that prefix. This is what catches install/export regressions (the
# §1.15 defects) which a build-and-ctest-only CI would otherwise never see.
add_test(
  NAME install_consumer_test
  COMMAND ${CMAKE_COMMAND}
    -DINSTALL_TEST_BUILD_DIR=${PROJECT_BINARY_DIR}
    -DINSTALL_TEST_CONSUMER_DIR=${CMAKE_CURRENT_SOURCE_DIR}/install_consumer
    -DINSTALL_TEST_PREFIX=${CMAKE_BINARY_DIR}/install_test_prefix
    -DINSTALL_TEST_BINARY_DIR=${CMAKE_BINARY_DIR}/install_test_consumer
    -DINSTALL_TEST_VERSION=${PROJECT_VERSION}
    -DINSTALL_TEST_CONFIG=$<CONFIG>
    -DINSTALL_TEST_TOOLCHAIN=${CMAKE_TOOLCHAIN_FILE}
    -DINSTALL_TEST_C_COMPILER=${CMAKE_C_COMPILER}
    -DINSTALL_TEST_C_FLAGS=${CMAKE_C_FLAGS}
    -DINSTALL_TEST_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
    -DINSTALL_TEST_EXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/install_consumer/driver.cmake
)
set_tests_properties(install_consumer_test PROPERTIES TIMEOUT 180)

# Compile-fail tests: each source deliberately uses an invalid input type for a
# macro in atomic_wait.h and must FAIL to compile, with the compiler emitting a
# specific diagnostic (DIAGNOSTIC is the expected message TEXT, matched as a
# literal substring by expect_compile_fail.cmake). Both C (.c) and C++ (.cpp)
# variants are checked. EXCLUDE_FROM_ALL keeps the targets out of the default
# build. Each test runs expect_compile_fail.cmake, which echoes the build output
# (so the diagnostic is visible), requires the build to fail, and exits non-zero
# unless the expected diagnostic text was emitted. Using our own static_assert
# message text matched literally (rather than a regex) keeps the check robust
# across gcc/clang/MSVC: no regex metacharacters to escape and no compiler-quoting
# differences to account for.
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
    wg14_atomic_waits_enable_c11_atomics(${name})
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
endfunction()

set(COMPILE_FAIL_DIR "compile_fail")
set(WIDTH_DIAGNOSTIC
    "wg14_atomic_waits: sizeof(*object) must be 1, 2, 4, or 8 bytes")
set(NATIVE_DIAGNOSTIC
    "wg14_atomic_waits: sizeof(*object) must equal "
    "sizeof(uint_native_wait_notify_t) (4 bytes)")
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
                      "${NATIVE_DIAGNOSTIC}")
add_compile_fail_test(compile_fail_atomic_notify_c
                      "${COMPILE_FAIL_DIR}/atomic_notify_badtype.c"
                      "${NATIVE_DIAGNOSTIC}")
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
                      "${NATIVE_DIAGNOSTIC}")
add_compile_fail_test(compile_fail_atomic_notify_cpp
                      "${COMPILE_FAIL_DIR}/atomic_notify_badtype.cpp"
                      "${NATIVE_DIAGNOSTIC}")
