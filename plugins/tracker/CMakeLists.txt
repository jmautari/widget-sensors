cmake_minimum_required(VERSION 3.15.1)

set(PROJECT_FOLDER "src")

# Project name.
project(tracker)

# Target executable names.
set(MAIN_TARGET "tracker")

file(GLOB_RECURSE ALL_SRCS
  ${PROJECT_FOLDER}/*.h
  ${PROJECT_FOLDER}/*.hpp
  ${PROJECT_FOLDER}/*.cpp
)

# Main executable sources.
set(MAIN_SRCS
  ${ALL_SRCS}
)

# Shared code components with the main project.
set(SHARED_SRCS
)

link_directories(
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
target_compile_definitions(${MAIN_TARGET} PRIVATE FMT_HEADER_ONLY OS_WIN _WIN32_IE=0x0600 _WIN32_WINNT=0x0A00 WINVER=0x0A00)

# Ignore warning nonstandard extension used: nameless struct/union
target_compile_options(${MAIN_TARGET} PRIVATE /wd4201 /wd4324)

target_link_options(${MAIN_TARGET} PRIVATE "$<$<CONFIG:RELEASE>:/DEBUG>" "$<$<CONFIG:RELEASE>:/OPT:REF>")

# Add required libs below
target_link_libraries(${MAIN_TARGET} PRIVATE
)
