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
set_target_properties(header_only_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                      "${CMAKE_BINARY_DIR}/bin")
add_test(NAME header_only_test COMMAND $<TARGET_FILE:header_only_test>)
