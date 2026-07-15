cmake_minimum_required(VERSION 3.28)
project(MacroTerminal VERSION 0.4.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)

add_executable(macro-terminal src/main.cpp)
target_compile_features(macro-terminal PUBLIC cxx_std_23)
target_include_directories(macro-terminal PRIVATE src)
target_link_libraries(macro-terminal PRIVATE
    glfw glad::glad imgui::imgui nlohmann_json::nlohmann_json
    Eigen3::Eigen CURL::libcurl)

if (UNIX AND NOT APPLE)
    target_link_libraries(macro-terminal PRIVATE dl pthread)
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(macro-terminal PRIVATE -fcoroutines)
endif()
if (MSVC)
    target_compile_definitions(macro-terminal PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

file(COPY ${CMAKE_SOURCE_DIR}/data DESTINATION ${CMAKE_BINARY_DIR}/bin)
