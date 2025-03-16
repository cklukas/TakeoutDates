cmake_minimum_required(VERSION 3.10)
project(GooglePhotosDateSetter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch nlohmann_json automatically
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3  # Latest stable version as of now
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(google_photos_date_setter main.cpp)
target_link_libraries(google_photos_date_setter PRIVATE nlohmann_json::nlohmann_json)