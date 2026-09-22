cmake_minimum_required(VERSION 3.24)
project(little_world VERSION 0.5.0 LANGUAGES CXX)

set(filament_root "${CMAKE_CURRENT_SOURCE_DIR}/vendor/sengine/.deps/filament" CACHE PATH "Filament SDK")
add_subdirectory(vendor/sengine EXCLUDE_FROM_ALL)
add_subdirectory(vendor/simulates EXCLUDE_FROM_ALL)

file(GLOB_RECURSE game_sources CONFIGURE_DEPENDS src/*.cpp)
add_executable(little_world ${game_sources})
target_compile_features(little_world PRIVATE cxx_std_23)
target_include_directories(little_world PRIVATE src src/app src/audio src/game src/render src/simulation src/ui)
target_link_libraries(little_world PRIVATE sengine::sengine simulates::simulates)
target_compile_options(little_world PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(little_world PROPERTIES CXX_EXTENSIONS OFF)

file(COPY assets/ DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/data")
install(TARGETS little_world RUNTIME DESTINATION .)
install(DIRECTORY assets/ DESTINATION data)
