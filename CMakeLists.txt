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

set(LIBRARY_SOURCES
  $<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${ALWAYS_USE_PTHREADS_BACKEND}>>>:src/wg14_atomic_waits/atomic_wait_windows.c>
  $<$<AND:$<PLATFORM_ID:Darwin>,$<NOT:$<BOOL:${ALWAYS_USE_PTHREADS_BACKEND}>>>:src/wg14_atomic_waits/atomic_wait_macos.c>
  $<$<AND:$<PLATFORM_ID:Linux>,$<NOT:$<BOOL:${ALWAYS_USE_PTHREADS_BACKEND}>>>:src/wg14_atomic_waits/atomic_wait_linux.c>
  $<$<AND:$<PLATFORM_ID:FreeBSD>,$<NOT:$<BOOL:${ALWAYS_USE_PTHREADS_BACKEND}>>>:src/wg14_atomic_waits/atomic_wait_freebsd.c>
  $<$<OR:$<NOT:$<OR:$<PLATFORM_ID:Windows>,$<PLATFORM_ID:Darwin>,$<PLATFORM_ID:Linux>,$<PLATFORM_ID:FreeBSD>>>,$<BOOL:${ALWAYS_USE_PTHREADS_BACKEND}>>:src/wg14_atomic_waits/atomic_wait_pthreads.c>
)

add_library(${PROJECT_NAME} ${LIBRARY_SOURCES})
target_link_libraries(${PROJECT_NAME} PUBLIC $<$<OR:$<NOT:$<OR:$<PLATFORM_ID:Windows>,$<PLATFORM_ID:Darwin>,$<PLATFORM_ID:Linux>,$<PLATFORM_ID:FreeBSD>>>,$<BOOL:${ALWAYS_USE_PTHREADS_BACKEND}>>:pthread>)
if(HEADER_ONLY_BUILD)
  target_compile_definitions(${PROJECT_NAME} PUBLIC WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY)
endif()
target_compile_features(${PROJECT_NAME} PRIVATE c_std_11 cxx_std_11)
target_compile_definitions(${PROJECT_NAME} PRIVATE WG14_ATOMIC_WAITS_SOURCE)
if(NOT MSVC)
  target_compile_definitions(${PROJECT_NAME} PRIVATE _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700 _GNU_SOURCE)
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
    target_link_libraries(${target} PRIVATE ${PROJECT_NAME})
    set_target_properties(${target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
      ${ADD_EXAMPLE_PROPERTIES}
    )
  endfunction()
  function(add_code_test target)
    add_code_example(${target} ${ARGN})
    add_test(NAME ${target} COMMAND $<TARGET_FILE:${target}>)
  endfunction()

  add_subdirectory("test")
endif()
