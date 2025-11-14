cmake_minimum_required(VERSION 3.20)
project(workout-tracker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# If yaml-cpp is vendored as a submodule:
# add_subdirectory(third_party/yaml-cpp)
# find_package will succeed if you have a system yaml-cpp; otherwise use add_subdirectory.
find_package(yaml-cpp QUIET)
if(NOT yaml-cpp_FOUND)
  #add_subdirectory(third_party/yaml-cpp)
endif()

add_executable(workout-tracker
  src/main.cpp
  src/app/App.cpp
  src/app/Config.cpp
  src/comm/BLEClient.cpp
  src/inference/PoseEstimator.cpp
  src/control/MotorController.cpp
)

target_include_directories(workout-tracker PRIVATE include)
target_link_libraries(workout-tracker
  PRIVATE
    yaml-cpp
    pthread
)

# Nice-to-have: put runtime in build/ for run_local.sh
set_target_properties(workout-tracker PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})


# sdbus-c++ (system or vendored)
find_package(sdbus-c++ REQUIRED)  # or add_subdirectory(third_party/sdbus-cpp)
target_link_libraries(workout-tracker
  PRIVATE
    yaml-cpp
    sdbus-c++
    pthread
)

target_link_libraries(workout-tracker PRIVATE gpiod)