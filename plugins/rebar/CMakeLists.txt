cmake_minimum_required(VERSION 3.15.1)

set(PROJECT_FOLDER "src")

# Project name.
project(rebar)

# Target executable names.
set(MAIN_TARGET "rebar")

file(GLOB_RECURSE ALL_SRCS
  ${PROJECT_FOLDER}/*.h
  ${PROJECT_FOLDER}/*.hpp
  ${PROJECT_FOLDER}/*.cpp
)

set(RESOURCES_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/win/resource.h
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/win/windows.rc
  )
  
# Main executable sources.
set(MAIN_SRCS
  ${ALL_SRCS}
  ${RESOURCES_SRCS}
)

# Shared code components with the main project.
set(SHARED_SRCS
)

link_directories(
  ${CMAKE_SOURCE_DIR}/third_party/nvapi/amd64
)

# Manifest files used for executables.
set(MAIN_EXE_MANIFEST
  resources/win/main.exe.manifest
  )

message(STATUS "Creating project")

#
# Windows configuration.
#

# Add resources to the sources variable for convenience.
set(MAIN_SRCS
  ${MAIN_SRCS}
  ${SHARED_SRCS}
  ${PROJECT_FOLDER}/plugin.def
  )

# Create source groups for Visual Studio.
#message(STATUS "Doing the src groups")
SET_PLUGIN_SOURCE_GROUPS("${MAIN_SRCS}")

# Executable target.
add_library(${MAIN_TARGET} SHARED ${MAIN_SRCS})
SET_PLUGIN_LIBRARY_TARGET_PROPERTIES(${MAIN_TARGET})

# Set additional compile & link options
target_compile_options(${MAIN_TARGET} PRIVATE "$<$<CONFIG:DEBUG>:/MDd>")
target_compile_options(${MAIN_TARGET} PRIVATE "$<$<CONFIG:RELEASE>:/MD>" "$<$<CONFIG:RELEASE>:/Zi>")
target_compile_definitions(${MAIN_TARGET} PRIVATE FMT_HEADER_ONLY OS_WIN)

# Ignore warning nonstandard extension used: nameless struct/union
target_compile_options(${MAIN_TARGET} PRIVATE /wd4201 /wd4324)

target_link_options(${MAIN_TARGET} PRIVATE "$<$<CONFIG:RELEASE>:/DEBUG>" "$<$<CONFIG:RELEASE>:/OPT:REF>")

# Add required libs below
target_link_libraries(${MAIN_TARGET} PRIVATE
  crypt32 nvapi64
)

add_custom_command(
  TARGET ${MAIN_TARGET}
  POST_BUILD
  COMMAND "mt.exe" -nologo
          -manifest \"${CMAKE_CURRENT_SOURCE_DIR}/${MAIN_EXE_MANIFEST}\"
          -outputresource:"${APP_TARGET_OUT_DIR}/${MAIN_TARGET}.dll"\;\#1
  COMMENT "Adding manifest..."
  )