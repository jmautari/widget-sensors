cmake_minimum_required(VERSION 3.15.1)

# Only generate Debug and Release configuration types.
set(CMAKE_CONFIGURATION_TYPES Debug Release)

set(PROJECT_FOLDER "power")

# Project name.
project(power)

# Target executable names.
set(MAIN_TARGET "power")

set(OS_WINDOWS 1)

message(STATUS "${PROJECT_FOLDER}")

# Main executable sources.
set(RESOURCES_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/win/resource.h
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/win/windows.rc
  )

file(GLOB_RECURSE MAIN_SRCS
  ${CMAKE_SOURCE_DIR}/${PROJECT_FOLDER}/*.h
  ${CMAKE_SOURCE_DIR}/${PROJECT_FOLDER}/*.hpp
  ${CMAKE_SOURCE_DIR}/${PROJECT_FOLDER}/*.cpp
  ${RESOURCES_SRCS}
)

# Shared code components with the main project.
file(GLOB_RECURSE SHARED_SRCS
  ${CMAKE_SOURCE_DIR}/shared/*.h
  ${CMAKE_SOURCE_DIR}/shared/*.hpp
  ${CMAKE_SOURCE_DIR}/shared/*.cpp
)

# Add additional include directories
include_directories(
  ${CMAKE_SOURCE_DIR}/power
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/third_party/json/single_include
)

# Allow relative include paths
include_directories(./ ../)

message(STATUS "Creating project")

# Support nice project/target organization in Visual Studio
# Enable the creation of project folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set the configuration-specific binary output directory.
if(GEN_NINJA OR GEN_MAKEFILES)
  # Force Ninja and Make to create a subdirectory named after the configuration.
  set(APP_TARGET_OUT_DIR "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")
else()
  set(APP_TARGET_OUT_DIR "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
endif()

# Group target contents by directory.
macro(SET_APP_SOURCE_GROUPS srcs)
  foreach(FILE ${srcs})
    # Get the absolute directory path.
    get_filename_component(ABS_FILE "${FILE}" ABSOLUTE)
    get_filename_component(PARENT_DIR "${ABS_FILE}" DIRECTORY)

    # Remove the common directory prefix. What remains in the group.
    string(REPLACE "${CMAKE_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")

    # Convert to Windows slashes.
    string(REPLACE "/" "\\" GROUP "${GROUP}")

    source_group("${GROUP}" FILES "${FILE}")

    message(STATUS "Added ${FILE} to group=${GROUP}")
  endforeach()
endmacro()

# Set properties on an app executable target.
macro(SET_APP_EXECUTABLE_TARGET_PROPERTIES target)
  # Output all binaries to the configuration-specific build directory.
  set_target_properties(${target} PROPERTIES
                        ARCHIVE_OUTPUT_DIRECTORY "${APP_TARGET_OUT_DIR}"
                        RUNTIME_OUTPUT_DIRECTORY "${APP_TARGET_OUT_DIR}"
                        LIBRARY_OUTPUT_DIRECTORY "${APP_TARGET_OUT_DIR}")
  target_compile_features(${target} PRIVATE cxx_std_17)

  if(OS_MACOSX OR OS_WINDOWS)
    # Place the target in the "getthumb" folder in Visual Studio and Xcode.
    set_property(TARGET ${target} PROPERTY FOLDER "${PROJECT_FOLDER}")
  endif()
endmacro()

#
# Windows configuration.
#

# Add resources to the sources variable for convenience.
set(MAIN_SRCS
  ${MAIN_SRCS}
  ${SHARED_SRCS}
  )

# Create source groups for Visual Studio.
#message(STATUS "Doing the src groups")
SET_APP_SOURCE_GROUPS("${MAIN_SRCS}")

# Manifest files used for executables.
set(MAIN_EXE_MANIFEST
  resources/win/main.exe.manifest
  )

add_definitions(-DUNICODE -D_UNICODE)

# Executable target.
add_executable(${MAIN_TARGET} WIN32 ${MAIN_SRCS})
SET_APP_EXECUTABLE_TARGET_PROPERTIES(${MAIN_TARGET})

# Set additional compile & link options
target_compile_options(${MAIN_TARGET} PRIVATE "$<$<CONFIG:DEBUG>:/MDd>")
target_compile_options(${MAIN_TARGET} PRIVATE "$<$<CONFIG:RELEASE>:/MD>" "$<$<CONFIG:RELEASE>:/Zi>")
target_compile_definitions(${MAIN_TARGET} PRIVATE
  FMT_HEADER_ONLY
  OS_WIN
  _WIN32_IE=0x0600
  _WIN32_WINNT=0x0A00
  WINVER=0x0A00
  LOG_DIR="widgets"
  LOG_FILE="power.log"
  )

set_target_properties(${MAIN_TARGET} PROPERTIES LINK_FLAGS /SUBSYSTEM:WINDOWS)

# Ignore warning nonstandard extension used: nameless struct/union
target_compile_options(${MAIN_TARGET} PRIVATE /wd4201 /wd4324)

target_link_options(${MAIN_TARGET} PRIVATE "$<$<CONFIG:RELEASE>:/DEBUG>" "$<$<CONFIG:RELEASE>:/OPT:REF>")

# Add required libs below
target_link_libraries(${MAIN_TARGET} PRIVATE
)

# Add the custom Windows manifest files to the executable.
add_custom_command(
  TARGET ${MAIN_TARGET}
  POST_BUILD
  COMMAND "mt.exe" -nologo
          -manifest \"${CMAKE_CURRENT_SOURCE_DIR}/${MAIN_EXE_MANIFEST}\"
          -outputresource:"${APP_TARGET_OUT_DIR}/${MAIN_TARGET}.exe"\;\#1
  COMMENT "Adding manifest..."
  )
