# This is a CMake file for building/including Tuvok into your own projects.
# It is meant only as a guide; it sets up only basic options which are specific
# to Tuvok.  If you need additional settings, it's recommended that you create
# an external file and use CMake's `include' directive to include this file
# inline.
# Still, there are some mechanisms setup to make that task easy.
#   . Use TUVOK_EXTERNAL_INCLUDES to setup a list of external include
#     directories.  This is useful if you want to, say, replace Tuvok's
#     internal copy of GLEW/boost with your own.  This directory gets included
#     *before* Tuvok's own directories.
#   . Set TUVOK_IGNORE to a CMake-list of files one doesn't want to use/compile
#     in their project.

cmake_minimum_required(VERSION 2.6)
if(COMMAND CMAKE_POLICY)
  cmake_policy(VERSION 2.6)
endif(COMMAND CMAKE_POLICY)

project(tuvok)

find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})
include_directories(${QT_INCLUDE_DIR})

include_directories(. ${TUVOK_EXTERNAL_INCLUDES} Basics Controller DebugOut IO
                    Renderer Scripting 3rdParty/GLEW IO/3rdParty/boost
                    IO/exception IO/expressions
                    ${QT_INCLUDE_DIR})

# Tuvok will not build as a shared lib.
set(BUILD_SHARED_LIBS OFF)

set(TUVOK_LIBRARY tuvok CACHE FILEPATH "Tuvok library name.")
set(TUVOK_INCLUDE
    "${CMAKE_CURRENT_SOURCE_DIR};${CMAKE_CURRENT_SOURCE_DIR}/3rdParty;${CMAKE_CURRENT_SOURCE_DIR}/IO/3rdParty/boost;${QT_INCLUDE_DIR}"
    CACHE STRING "Tuvok include directory (Tuvok's source tree root)" FORCE)
# fPIC is required for libraries.
if(${CMAKE_COMPILER_IS_GNUCXX})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif(${CMAKE_COMPILER_IS_GNUCXX})

# Disable Qt as well.  Needed for now... working on it, slowly.
#set(TUVOK_DEFINES -DTUVOK_NO_IO -DTUVOK_NO_QT CACHE STRING
set(TUVOK_DEFINES -DTUVOK_NO_IO CACHE STRING
    "Defines needed for building Tuvok in most projects.")

# Disable IO and Qt within Tuvok.
add_definitions(${TUVOK_DEFINES})

# Sets the variable TUVOK_SRCS.
include(.tuvok_srcs)

if(DEFINED TUVOK_IGNORE)
  list(REMOVE_ITEM TUVOK_SRCS TUVOK_IGNORE)
endif(DEFINED TUVOK_IGNORE)

add_library(${TUVOK_LIBRARY} ${TUVOK_SRCS})
# We try to read shaders from the bundle on Macs, so we need CoreFoundation.
if(APPLE)
  target_link_libraries(${TUVOK_LIBRARY} "-framework CoreFoundation")
endif(APPLE)
target_link_libraries(${TUVOK_LIBRARY} ${QT_LIBRARIES})
target_link_libraries(${TUVOK_LIBRARY} ${GL_LIBRARY})
