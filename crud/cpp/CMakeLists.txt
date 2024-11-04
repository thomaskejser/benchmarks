cmake_minimum_required(VERSION 3.10)

project(crud)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


set(BASE_PATH $ENV{SRC_PATH})

if(NOT BASE_PATH)
    message(FATAL_ERROR "Environment variable SRC_PATH is not set. Please set it to the base of where I can find
    external sources")
endif()

set(RAPIDJSON_INCLUDE_DIR "${BASE_PATH}/rapidjson/include")
include_directories(${RAPIDJSON_INCLUDE_DIR})

set(NLOHMANN_INCLUDE_DIR "${BASE_PATH}/json/single_include")
include_directories(${NLOHMANN_INCLUDE_DIR})


add_executable(crud src/main.cpp)


if(WIN32)
    target_link_libraries(crud Shlwapi)
endif()

if(WIN32)
    target_compile_definitions(crud PRIVATE WIN32_LEAN_AND_MEAN)
endif()

set_target_properties(crud PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
