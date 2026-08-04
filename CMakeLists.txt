cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
set(CMAKE_C_VISIBILITY_PRESET "hidden" CACHE STRING "Only make explicitly exported symbols visible")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden" CACHE STRING "Only make explicitly exported symbols visible")
set(CMAKE_C_STANDARD "11" CACHE STRING "C standard to use")
option(HEADER_ONLY_BUILD "Build using header only (unity build)" OFF)
option(ALWAYS_USE_PTHREADS_BACKEND "Always use pthreads backend" OFF)

project(wg14_atomic_waits LANGUAGES C CXX)
include(GNUInstallDirs)
enable_testing()

# MSVC provides C11 atomics (<stdatomic.h> -> vcruntime_c11_stdatomic.h) only
# when atomics support is enabled. VS2022 17.5+ (MSVC >= 19.35) enables them
# for /std:c11 without an extra flag, but older toolchains additionally require
# /experimental:c11atomics; without it the header fails with
#   vcruntime_c11_stdatomic.h: error C1189: #error: "C atomic support is not enabled"
function(wg14_atomic_waits_enable_c11_atomics target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /experimental:c11atomics)
  endif()
endfunction()

set(LIBRARY_SOURCES
  $<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${ALWAYS_USE_PTHREADS_BACKEND}>>>:src/wg14_atomic_waits/atomic_wait_windows.c>
  $<$<AND:$<PLATFORM_ID:Darwin>,$<NOT:$<BOOL:${ALWAYS_USE_PTHREADS_BACKEND}>>>:src/wg14_atomic_waits/atomic_wait_macos.c>
  $<$<AND:$<PLATFORM_ID:Linux>,$<NOT:$<BOOL:${ALWAYS_USE_PTHREADS_BACKEND}>>>:src/wg14_atomic_waits/atomic_wait_linux.c>
  $<$<AND:$<PLATFORM_ID:FreeBSD>,$<NOT:$<BOOL:${ALWAYS_USE_PTHREADS_BACKEND}>>>:src/wg14_atomic_waits/atomic_wait_freebsd.c>
  $<$<OR:$<NOT:$<OR:$<PLATFORM_ID:Windows>,$<PLATFORM_ID:Darwin>,$<PLATFORM_ID:Linux>,$<PLATFORM_ID:FreeBSD>>>,$<BOOL:${ALWAYS_USE_PTHREADS_BACKEND}>>:src/wg14_atomic_waits/atomic_wait_pthreads.c>
)

add_library(${PROJECT_NAME} ${LIBRARY_SOURCES})
target_link_libraries(${PROJECT_NAME} PUBLIC $<$<OR:$<NOT:$<OR:$<PLATFORM_ID:Windows>,$<PLATFORM_ID:Darwin>,$<PLATFORM_ID:Linux>,$<PLATFORM_ID:FreeBSD>>>,$<BOOL:${ALWAYS_USE_PTHREADS_BACKEND}>>:pthread>)
# WaitOnAddress/WakeByAddressSingle/WakeByAddressAll are provided by
# Synchronization.lib on the Windows SDK. Only the native Windows backend
# (not the forced-pthreads build) uses them.
target_link_libraries(${PROJECT_NAME} PUBLIC
  $<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${ALWAYS_USE_PTHREADS_BACKEND}>>>:Synchronization>)
if(HEADER_ONLY_BUILD)
  target_compile_definitions(${PROJECT_NAME} PUBLIC WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY)
endif()
target_compile_features(${PROJECT_NAME} PRIVATE c_std_11 cxx_std_11)
wg14_atomic_waits_enable_c11_atomics(${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME} PRIVATE WG14_ATOMIC_WAITS_SOURCE)
if(NOT MSVC)
  target_compile_definitions(${PROJECT_NAME} PRIVATE _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700 _GNU_SOURCE)
endif()
if(WIN32)
  # WaitOnAddress/WakeByAddress* require Windows 8 (0x0602) SDK declarations
  target_compile_definitions(${PROJECT_NAME} PUBLIC _WIN32_WINNT=0x0602 WINVER=0x0602)
endif()

# Suppress warnings inside backends that intentionally use volatile-to-void* cast:
if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}Exports
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

configure_file(
  "${CMAKE_CURRENT_LIST_DIR}/cmake/ProjectConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  @ONLY
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)
install(EXPORT ${PROJECT_NAME}Exports
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

if(PROJECT_IS_TOP_LEVEL AND (NOT DEFINED BUILD_TESTING OR BUILD_TESTING))
  function(add_code_example target)
    cmake_parse_arguments(ADD_EXAMPLE "" "" "SOURCES;FEATURES;PROPERTIES" ${ARGN})
    add_executable(${target} ${ADD_EXAMPLE_SOURCES})
    if(MSVC)
      target_compile_options(${target} PRIVATE /W4)
    else()
      target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_compile_features(${target} PRIVATE ${ADD_EXAMPLE_FEATURES})
    wg14_atomic_waits_enable_c11_atomics(${target})
    target_link_libraries(${target} PRIVATE ${PROJECT_NAME})
    # FreeBSD keeps its C11 <threads.h> implementation (thrd_*, mtx_*, ...) in
    # a separate base library, libstdthreads, rather than in libc or libpthread,
    # so executables using the C11 threads API must link it explicitly.
    target_link_libraries(${target} PRIVATE $<$<PLATFORM_ID:FreeBSD>:stdthreads>)
    set_target_properties(${target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
      ${ADD_EXAMPLE_PROPERTIES}
    )
  endfunction()
  function(add_code_test target)
    add_code_example(${target} ${ARGN})
    add_test(NAME ${target} COMMAND $<TARGET_FILE:${target}>)
    # Fail fast if a test hangs: a stuck wait/notify would otherwise burn the
    # CI-wide ctest --timeout (300s) before being reported as a timeout.
    set_tests_properties(${target} PROPERTIES TIMEOUT 60)
  endfunction()

  add_subdirectory("test")
endif()
