cmake_minimum_required(VERSION 3.20)
project(hft_crypto_engine LANGUAGES CXX)

# ---- Global compiler settings ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
#set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set policy for Boost find module removal
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# ---- Find system packages (brew installed) ----
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

# Find Boost with Beast (Beast is header-only, part of Boost since 1.66)
# We need system and thread for Beast WebSocket operations
find_package(Boost REQUIRED COMPONENTS system thread CONFIG QUIET)
if(NOT Boost_FOUND)
    find_package(Boost REQUIRED COMPONENTS system thread MODULE)
endif()

# Verify Boost version supports Beast
if(Boost_VERSION_STRING VERSION_LESS "1.66.0")
    message(FATAL_ERROR "Boost.Beast requires Boost 1.66.0 or higher. Found: ${Boost_VERSION_STRING}")
endif()

# ---- Sources ----
file(GLOB_RECURSE SOURCES
    ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_executable(hft_engine ${SOURCES})

# ---- Include paths ----
target_include_directories(hft_engine
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# ---- Compile definitions ----
# Beast requires these for proper compilation
target_compile_definitions(hft_engine
    PRIVATE
    BOOST_BEAST_USE_STD_STRING_VIEW
    BOOST_ASIO_HAS_STD_INVOKE_RESULT
)

# ---- Link libraries ----
target_link_libraries(hft_engine
    PRIVATE
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    Boost::thread
    pthread
)

# ---- Optimization flags ----
target_compile_options(hft_engine PRIVATE -O3 -march=native)

# ---- Additional compiler flags for Beast ----
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(hft_engine PRIVATE -fconcepts)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(hft_engine PRIVATE -stdlib=libc++)
endif()