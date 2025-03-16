cmake_minimum_required(VERSION 3.10)
project(TakeoutPhotosDateSetter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch nlohmann_json
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  BIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(takeout_photos_date_setter main.cpp)
target_link_libraries(takeout_photos_date_setter PRIVATE nlohmann_json::nlohmann_json)

if (APPLE)
    target_sources(takeout_photos_date_setter PRIVATE mac_tags.mm)
    target_link_libraries(takeout_photos_date_setter PRIVATE "-framework Foundation")
endif()

if (WIN32)
    target_link_libraries(takeout_photos_date_setter PRIVATE kernel32)
endif()