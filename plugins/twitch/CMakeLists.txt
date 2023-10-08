cmake_minimum_required(VERSION 3.15.1)

set(PROJECT_FOLDER "src")

# Project name.
project(twitch)

# Target executable names.
set(MAIN_TARGET "twitch")

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
file(GLOB_RECURSE SHARED_SRCS
  ${CMAKE_SOURCE_DIR}/shared/*.h
  ${CMAKE_SOURCE_DIR}/shared/*.hpp
  ${CMAKE_SOURCE_DIR}/shared/*.cpp
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
target_compile_definitions(${MAIN_TARGET} PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT FMT_HEADER_ONLY OS_WIN _WIN32_IE=0x0600 _WIN32_WINNT=0x0A00 WINVER=0x0A00 ASIO_STANDALONE _WEBSOCKETPP_CPP11_TYPE_TRAITS_ _WEBSOCKETPP_CPP11_RANDOM_DEVICE_)

message(STATUS ${CMAKE_SOURCE_DIR})

target_link_directories(${MAIN_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party/openssl/lib
  ${CMAKE_SOURCE_DIR}/third_party/nvapi/amd64
  )

# Ignore warning nonstandard extension used: nameless struct/union
target_compile_options(${MAIN_TARGET} PRIVATE /wd4201 /wd4324)

target_link_options(${MAIN_TARGET} PRIVATE "$<$<CONFIG:RELEASE>:/DEBUG>" "$<$<CONFIG:RELEASE>:/OPT:REF>")

# Add required libs below
target_link_libraries(${MAIN_TARGET} PRIVATE
  crypt32 libssl.lib libcrypto.lib nvapi64
)
