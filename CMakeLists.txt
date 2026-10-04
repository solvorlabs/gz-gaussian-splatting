cmake_minimum_required(VERSION 3.16)
project(gz_gaussian_splatting VERSION 0.1.0 LANGUAGES CXX)
include(GNUInstallDirs)
include(CTest)
option(BUILD_RENDERER "Build the Gazebo Harmonic Ogre2 GUI plugin" ON)
option(WITH_AMENT "Register as a ROS 2/colcon package" OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
add_library(gzgs_cache STATIC src/Cache.cc)
set_target_properties(gzgs_cache PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(gzgs_cache PUBLIC cxx_std_17)
target_include_directories(gzgs_cache PUBLIC include)
target_compile_options(gzgs_cache PRIVATE -Wall -Wextra -Wpedantic)

if(BUILD_RENDERER)
  # Discover either OSRF native libraries or ROS Jazzy vendor prefixes.
  find_package(gz_common_vendor QUIET)
  find_package(gz_gui_vendor QUIET)
  find_package(gz_plugin_vendor QUIET)
  find_package(gz_rendering_vendor QUIET)
  find_package(gz_msgs_vendor QUIET)
  find_package(gz_transport_vendor QUIET)
  find_package(gz_ogre_next_vendor QUIET)
  find_package(gz-common5 REQUIRED COMPONENTS graphics)
  find_package(gz-gui8 REQUIRED)
  find_package(gz-plugin2 REQUIRED COMPONENTS register)
  find_package(gz-rendering8 REQUIRED)
  find_package(gz-msgs10 REQUIRED)
  find_package(gz-transport13 REQUIRED)
  find_package(Qt5 REQUIRED COMPONENTS Core Quick)
  # No machine-specific /opt path. Users may set these cache variables for
  # unusual source installations. Prefix hints also cover Jazzy vendor dirs.
  find_path(OGRE_NEXT_INCLUDE_DIR OgreManualObject2.h
    HINTS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES include/OGRE-Next include/OGRE-2.3 include/OGRE)
  find_library(OGRE_NEXT_MAIN_LIBRARY NAMES OgreNextMain OgreMain
    HINTS ${CMAKE_PREFIX_PATH} PATH_SUFFIXES lib lib/x86_64-linux-gnu)
  find_library(GZ_RENDERING_OGRE2_LIBRARY NAMES gz-rendering-ogre2
    HINTS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES lib/gz-rendering-8/engine-plugins lib/x86_64-linux-gnu/gz-rendering-8/engine-plugins)
  if(NOT OGRE_NEXT_INCLUDE_DIR OR NOT OGRE_NEXT_MAIN_LIBRARY OR NOT GZ_RENDERING_OGRE2_LIBRARY)
    message(FATAL_ERROR "Ogre-Next development files or gz-rendering8 Ogre2 plugin missing. See docs/BUILD.md; set OGRE_NEXT_INCLUDE_DIR, OGRE_NEXT_MAIN_LIBRARY and GZ_RENDERING_OGRE2_LIBRARY for custom installs.")
  endif()
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)
  cmake_policy(SET CMP0100 NEW)
  add_library(GaussianSplatRenderer SHARED src/GaussianSplatRenderer.cc
    src/GaussianSplatRenderer.hh src/GaussianSplatRenderer.qrc)
  target_compile_features(GaussianSplatRenderer PRIVATE cxx_std_17)
  target_compile_definitions(GaussianSplatRenderer PRIVATE OGRE_DEBUG_MODE=0)
  target_compile_options(GaussianSplatRenderer PRIVATE -Wall -Wextra -Wpedantic)
  target_include_directories(GaussianSplatRenderer SYSTEM PRIVATE
    ${OGRE_NEXT_INCLUDE_DIR} ${OGRE_NEXT_INCLUDE_DIR}/Hlms/Common)
  target_link_libraries(GaussianSplatRenderer PRIVATE gzgs_cache
    gz-common5::graphics gz-gui8::core gz-plugin2::register gz-rendering8::core
    gz-msgs10::gz-msgs10 gz-transport13::gz-transport13
    ${GZ_RENDERING_OGRE2_LIBRARY} ${OGRE_NEXT_MAIN_LIBRARY} Qt5::Core Qt5::Quick)
  install(TARGETS GaussianSplatRenderer LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
if(BUILD_TESTING)
  add_executable(cache_test tests/cache_test.cc)
  target_link_libraries(cache_test PRIVATE gzgs_cache)
  add_test(NAME cache_contract COMMAND cache_test)
  add_executable(cache_probe tests/cache_probe.cc)
  target_link_libraries(cache_probe PRIVATE gzgs_cache)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_test(NAME python_contracts COMMAND ${Python3_EXECUTABLE} -m unittest discover
    -s ${CMAKE_CURRENT_SOURCE_DIR}/tests -p test_*.py -v)
  set_tests_properties(python_contracts PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/python;PYTHONDONTWRITEBYTECODE=1;GZGS_CACHE_PROBE=$<TARGET_FILE:cache_probe>")
endif()
install(DIRECTORY examples docs DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME})
install(FILES LICENSE NOTICE README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME})
if(WITH_AMENT)
  find_package(ament_cmake REQUIRED)
  ament_environment_hooks(env-hooks/gzgs.dsv.in)
  ament_package()
endif()
