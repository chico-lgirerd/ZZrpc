cmake_minimum_required(VERSION 3.16)
project(zzrpc_native CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

include(FetchContent)

# httplib is used only as a plain localhost HTTP server (see src/server.cpp) — no
# outbound requests, no TLS, no compression. By default it auto-links whatever
# OpenSSL/zlib/brotli/zstd it finds on the system, which on macOS drags in
# Homebrew/MacPorts libs that may be a different arch than the build and break the
# link. Pin every optional feature off so the build is hermetic on all platforms.
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF)
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF)

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

# the sdk zip ships a platform-specific shared lib in lib/release/, and the
# relocatable-rpath token differs: ELF uses $ORIGIN, Mach-O uses @loader_path.
# only macOS and Linux are supported — anything else is a hard error so the
# failure is obvious at configure time instead of a confusing link error later.
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  message(STATUS "zzrpc: building for macOS (Darwin)")
  set(DISCORD_SDK_LIB "${DISCORD_SDK_ROOT}/lib/release/libdiscord_partner_sdk.dylib")
  set(DISCORD_SDK_ORIGIN_RPATH "@loader_path")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(STATUS "zzrpc: building for Linux")
  set(DISCORD_SDK_LIB "${DISCORD_SDK_ROOT}/lib/release/libdiscord_partner_sdk.so")
  set(DISCORD_SDK_ORIGIN_RPATH "$ORIGIN")
else()
  message(FATAL_ERROR
    "zzrpc only supports macOS and Linux, but CMAKE_SYSTEM_NAME is "
    "'${CMAKE_SYSTEM_NAME}'. The Discord Social SDK ships no shared library "
    "for this platform.")
endif()

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
  # $ORIGIN (@loader_path on macOS) = the binary's own directory at runtime, not this
  # machine's absolute SDK path. BUILD_WITH_INSTALL_RPATH TRUE bakes that in immediately
  # (not just on `cmake --install`), so the dist/ folder is relocatable straight out of
  # the build, no root/install step needed.
  INSTALL_RPATH "${DISCORD_SDK_ORIGIN_RPATH}"
  BUILD_WITH_INSTALL_RPATH TRUE
)

# copies the sdk shared lib next to the binary so dist/ is self-contained and can be moved/zipped as-is
add_custom_command(TARGET zzrpc POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy "${DISCORD_SDK_LIB}" "${CMAKE_BINARY_DIR}/dist/"
)
