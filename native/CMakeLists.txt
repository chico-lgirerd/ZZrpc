cmake_minimum_required(VERSION 3.16)
project(zzrpc_native CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.18.3
)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# extracted from the sdk zip, gitignored, download it yourself from the dev portal
set(DISCORD_SDK_ROOT "${CMAKE_SOURCE_DIR}/discord_social_sdk")
set(DISCORD_SDK_INCLUDE_DIR "${DISCORD_SDK_ROOT}/include")
set(DISCORD_SDK_LIB "${DISCORD_SDK_ROOT}/lib/release/libdiscord_partner_sdk.so")

if(NOT EXISTS "${DISCORD_SDK_INCLUDE_DIR}")
  message(FATAL_ERROR
    "Discord Social SDK not found at ${DISCORD_SDK_ROOT}. "
    "Run ./fetch-sdk.sh for instructions, or see README.md.")
endif()

add_executable(zzrpc
  src/main.cpp
  src/auth.cpp
  src/presence.cpp
  src/server.cpp
  src/discordpp_impl.cpp
)

target_include_directories(zzrpc PRIVATE ${DISCORD_SDK_INCLUDE_DIR})
target_link_libraries(zzrpc PRIVATE httplib::httplib nlohmann_json::nlohmann_json "${DISCORD_SDK_LIB}")

set_target_properties(zzrpc PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
  # $ORIGIN = the binary's own directory at runtime, not this machine's absolute SDK path.
  # BUILD_WITH_INSTALL_RPATH TRUE bakes that in immediately (not just on `cmake --install`),
  # so the dist/ folder is relocatable straight out of the build, no root/install step needed.
  INSTALL_RPATH "$ORIGIN"
  BUILD_WITH_INSTALL_RPATH TRUE
)

# copies the .so next to the binary so dist/ is self-contained and can be moved/zipped as-is
add_custom_command(TARGET zzrpc POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy "${DISCORD_SDK_LIB}" "${CMAKE_BINARY_DIR}/dist/"
)
