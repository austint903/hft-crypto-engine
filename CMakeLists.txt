cmake_minimum_required(VERSION 3.20)
project(hft_crypto_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

find_package(OpenSSL REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

find_package(Boost REQUIRED COMPONENTS system thread CONFIG QUIET)
if(NOT Boost_FOUND)
    find_package(Boost REQUIRED COMPONENTS system thread MODULE)
endif()

if(Boost_VERSION_STRING VERSION_LESS "1.66.0")
    message(FATAL_ERROR "Boost.Beast requires Boost 1.66.0 or higher. Found: ${Boost_VERSION_STRING}")
endif()

file(GLOB_RECURSE SOURCES
    ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_executable(hft_engine ${SOURCES})

target_include_directories(hft_engine
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)


target_compile_definitions(hft_engine
    PRIVATE
    BOOST_BEAST_USE_STD_STRING_VIEW
    BOOST_ASIO_HAS_STD_INVOKE_RESULT
)

target_link_libraries(hft_engine
    PRIVATE
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    Boost::thread
    pthread
)

target_compile_options(hft_engine PRIVATE -O3 -march=native)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(hft_engine PRIVATE -fconcepts)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(hft_engine PRIVATE -stdlib=libc++)
endif()