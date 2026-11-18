# Fails if a public header directly includes a system header that is not
# available in a conforming freestanding C implementation (C2y 4p6). The
# transitive header-only case is covered by freestanding_no_timeout_test.
# <stdatomic.h> is allowed because this extension is part of the atomics API;
# <atomic> is allowed for the C++ branch.

if(NOT DEFINED HEADER_DIR)
  message(FATAL_ERROR "HEADER_DIR must be defined")
endif()

set(allowed_headers
    float.h
    iso646.h
    limits.h
    stdalign.h
    stdarg.h
    stdbit.h
    stdbool.h
    stdcountof.h
    stddef.h
    stdint.h
    stdnoreturn.h
    stdatomic.h
    atomic)

foreach(header atomic_wait.h config.h)
  file(STRINGS "${HEADER_DIR}/wg14_atomic_waits/${header}" lines REGEX
       "#[ \t]*include")
  foreach(line IN LISTS lines)
    string(REGEX MATCHALL "<[^>]+>" matches "${line}")
    foreach(match IN LISTS matches)
      string(REGEX REPLACE "^<" "" name "${match}")
      string(REGEX REPLACE ">$" "" name "${name}")
      list(FIND allowed_headers "${name}" index)
      if(index EQUAL -1)
        message(FATAL_ERROR
                "${header}: includes non-freestanding header <${name}>: ${line}")
      endif()
    endforeach()
  endforeach()
endforeach()

message(STATUS "Public headers include only freestanding C headers")
