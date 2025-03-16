cmake_minimum_required(VERSION 3.10)
project(GooglePhotosDateSetter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find nlohmann_json
find_package(nlohmann_json 3.2.0 REQUIRED)

add_executable(google_photos_date_setter main.cpp)
target_link_libraries(google_photos_date_setter PRIVATE nlohmann_json::nlohmann_json)
