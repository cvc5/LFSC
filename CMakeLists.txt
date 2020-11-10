cmake_minimum_required (VERSION 2.8.9)

macro(add_cxx_flag flag)
  message(STATUS "Configure with flag '${flag}'")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
endmacro()

project (lfsc_checker)

set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package(GMP REQUIRED)
set(LIBRARIES ${LIBRARIES} ${GMP_LIBRARIES})
include_directories(${GMP_INCLUDE_DIR})

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Defaulting to release build.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_cxx_flag("-Wall")
add_cxx_flag("-Wno-deprecated")
add_cxx_flag("-std=gnu++11")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_cxx_flag("-g")
  message(STATUS "Configured debug build.")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  message(STATUS "Configured release build.")
else()
  message(FATAL_ERROR "Invalid build type '${CMAKE_BUILD_TYPE}'")
endif()

find_package(FLEX REQUIRED)

if (CMAKE_HOST_SYSTEM_NAME MATCHES "Darwin")
  execute_process(
        COMMAND brew --prefix flex 
        RESULT_VARIABLE BREW_FLEX
        OUTPUT_VARIABLE BREW_FLEX_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if (BREW_FLEX EQUAL 0 AND EXISTS "${BREW_FLEX_PREFIX}")
    set(FLEX_EXECUTABLE "${BREW_FLEX_PREFIX}/bin/flex")
    set(FLEX_INCLUDE_DIRS "${BREW_FLEX_PREFIX}/include")
  else()
    message(FATAL_ERROR "Could not find flex for macOS!")
    exit()
  endif()
endif()

if (FLEX_FOUND AND NOT ${FLEX_INCLUDE_DIRS} STREQUAL "FLEX_INCLUDE_DIR-NOTFOUND")
    message(STATUS "Flex include directories: ${FLEX_INCLUDE_DIRS}")
    message(STATUS "Flex executable: ${FLEX_EXECUTABLE}")
    message(STATUS "Flex version: ${FLEX_VERSION}")
else()
  message(FATAL_ERROR "Could not find flex!")
  exit()
endif()

enable_testing()

include_directories(src)
add_subdirectory(src)
add_subdirectory(tests)

