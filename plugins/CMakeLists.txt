cmake_minimum_required(VERSION 3.15.1)

# Only generate Debug and Release configuration types.
set(CMAKE_CONFIGURATION_TYPES Debug Release)

# Project name.
project(widget-plugins)

set(OS_WINDOWS 1)

add_definitions(-DUNICODE -D_UNICODE)

# Support nice project/target organization in Visual Studio
# Enable the creation of project folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Group target contents by directory.
macro(SET_PLUGIN_SOURCE_GROUPS srcs)
  foreach(FILE ${srcs})
    # Get the absolute directory path.
    get_filename_component(ABS_FILE "${FILE}" ABSOLUTE)
    get_filename_component(PARENT_DIR "${ABS_FILE}" DIRECTORY)

    message(STATUS "${CMAKE_CURRENT_SOURCE_DIR}/")
    message(STATUS "${PARENT_DIR}")
    
    # Remove the common directory prefix. What remains in the group.
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" GROUP "${PARENT_DIR}")

    # Convert to Windows slashes.
    string(REPLACE "/" "\\" GROUP "${GROUP}")

    source_group("${GROUP}" FILES "${FILE}")

    message(STATUS "Added ${FILE} to group=${GROUP}")
  endforeach()
endmacro()

set(TARGET_OUT_DIR ${CMAKE_BINARY_DIR})

# Set properties on an app library target.
macro(SET_PLUGIN_LIBRARY_TARGET_PROPERTIES target)
  # Output all binaries to the configuration-specific build directory.
  set_target_properties(${target} PROPERTIES
                        ARCHIVE_OUTPUT_DIRECTORY "${TARGET_OUT_DIR}"
                        RUNTIME_OUTPUT_DIRECTORY "${TARGET_OUT_DIR}"
                        LIBRARY_OUTPUT_DIRECTORY "${TARGET_OUT_DIR}")
  target_compile_features(${target} PRIVATE cxx_std_17)
  set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)

  if(OS_WINDOWS)
    set_property(TARGET ${target} PROPERTY FOLDER "${PROJECT_FOLDER}")
  endif()
endmacro()

MACRO(SUBDIRLIST result curdir)
  FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
  SET(dirlist "")
  FOREACH(child ${children})
    IF(IS_DIRECTORY ${curdir}/${child})
      LIST(APPEND dirlist ${child})
    ENDIF()
  ENDFOREACH()
  SET(${result} ${dirlist})
ENDMACRO()

SUBDIRLIST(SUBDIRS "${CMAKE_CURRENT_SOURCE_DIR}")

FOREACH(subdir ${SUBDIRS})
  message(STATUS "Adding plugin ${subdir}")
  add_subdirectory(${subdir})
ENDFOREACH()
