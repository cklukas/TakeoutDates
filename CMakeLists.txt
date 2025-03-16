cmake_minimum_required(VERSION 3.10)
project(GooglePhotosDateSetter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch nlohmann_json
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Use .mm extension for Objective-C++
add_executable(google_photos_date_setter main.mm)
target_link_libraries(google_photos_date_setter PRIVATE nlohmann_json::nlohmann_json)
if (APPLE)
    target_link_libraries(google_photos_date_setter PRIVATE "-framework Foundation")
endif()