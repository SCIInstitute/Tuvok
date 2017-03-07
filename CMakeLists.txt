
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(Tuvok VERSION 3.1.0)

if(NOT TUVOK_QUIET)
  message(WARNING "Building Tuvok for ImageVis3D is only supported through the "
                  "qmake build. This build is for embedded Tuvok; see the "
                  "<bbp-open-source@googlegroups.com> mailing list for support.")
endif()

find_package(BISON REQUIRED QUIET)
find_package(FLEX REQUIRED QUIET)
find_package(GLEW REQUIRED QUIET)
find_package(OpenGL REQUIRED QUIET)
find_package(OpenMP QUIET)
find_package(Qt5OpenGL REQUIRED QUIET)
find_package(ZLIB REQUIRED QUIET)

# Expressions library
BISON_TARGET(Parser IO/expressions/tvk-parse.ypp ${PROJECT_BINARY_DIR}/tvk-parse.cpp)
FLEX_TARGET(Scanner IO/expressions/tvk-scan.lpp ${PROJECT_BINARY_DIR}/tvk-scan.cpp)
ADD_FLEX_BISON_DEPENDENCY(Scanner Parser)

set(TUVOK_EXPRESSION_SOURCES ${PROJECT_BINARY_DIR}/tvk-scan.cpp
                             ${PROJECT_BINARY_DIR}/tvk-parse.cpp
                             IO/expressions/binary-expression.cpp
                             IO/expressions/conditional-expression.cpp
                             IO/expressions/constant.cpp
                             IO/expressions/treenode.cpp
                             IO/expressions/volume.cpp )
add_library(TuvokExpressions SHARED ${TUVOK_EXPRESSION_SOURCES})
set_property(TARGET TuvokExpressions PROPERTY CXX_STANDARD 11)
target_compile_options(TuvokExpressions PRIVATE -fno-strict-aliasing)
target_include_directories(TuvokExpressions PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/Basics>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/IO/expressions>")
install(TARGETS TuvokExpressions EXPORT TuvokTargets LIBRARY DESTINATION lib)

# Tuvok library
include(files.cmake)
set_source_files_properties(IO/3rdParty/tiff/tif_unix.c PROPERTIES COMPILE_FLAGS
  "-Wno-pointer-to-int-cast -Wno-int-to-pointer-cast")
add_library(Tuvok SHARED ${TUVOK_HEADERS} ${TUVOK_SRCS})
target_link_libraries(Tuvok PUBLIC ${OPENGL_LIBRARIES} TuvokExpressions PRIVATE Qt5::OpenGL
    ${GLEW_LIBRARIES} ${ZLIB_LIBRARY})
if(APPLE)
  find_library(COREFOUNDATION_LIB CoreFoundation)
  target_link_libraries(Tuvok ${COREFOUNDATION_LIB})
endif()
set_property(TARGET Tuvok PROPERTY CXX_STANDARD 11)
target_compile_options(Tuvok PRIVATE -fno-strict-aliasing ${OpenMP_CXX_FLAGS})
set_property(TARGET Tuvok APPEND PROPERTY LINK_FLAGS ${OpenMP_CXX_FLAGS})
target_include_directories(Tuvok PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/..>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/Basics>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/IO/exception>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/IO/3rdParty/lzma>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/IO/3rdParty/lz4>")
install(TARGETS Tuvok EXPORT TuvokTargets
        LIBRARY DESTINATION lib INCLUDES DESTINATION include include/Tuvok)
install(FILES StdTuvokDefines.h DESTINATION include/Tuvok)
install(DIRECTORY Basics Controller DebugOut IO LuaScripting
        DESTINATION include/Tuvok FILES_MATCHING PATTERN "*.h")

# TuvokConfig.cmake and TuvokTargets.cmake magic to allow find_package(Tuvok)
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/TuvokConfigVersion.cmake"
  COMPATIBILITY SameMajorVersion
)
if(MSVC)
  set(CMAKE_MODULE_INSTALL_PATH ${PROJECT_NAME}/CMake)
else()
  set(CMAKE_MODULE_INSTALL_PATH share/${PROJECT_NAME}/CMake)
endif()

if(NOT TARGET Tuvok_ALIAS)
  add_library(Tuvok_ALIAS ALIAS Tuvok)
endif()

configure_package_config_file(${CMAKE_CURRENT_LIST_DIR}/CMake/TuvokConfig.cmake.in
  ${PROJECT_BINARY_DIR}/TuvokConfig.cmake
  INSTALL_DESTINATION ${CMAKE_MODULE_INSTALL_PATH}
)
install(FILES ${PROJECT_BINARY_DIR}/TuvokConfig.cmake
              ${PROJECT_BINARY_DIR}/TuvokConfigVersion.cmake
        DESTINATION ${CMAKE_MODULE_INSTALL_PATH}
)

export(EXPORT TuvokTargets FILE ${PROJECT_BINARY_DIR}/TuvokTargets.cmake)
install(EXPORT TuvokTargets FILE TuvokTargets.cmake
        DESTINATION ${CMAKE_MODULE_INSTALL_PATH}
)
