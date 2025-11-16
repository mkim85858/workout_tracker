cmake_minimum_required(VERSION 3.20)
project(workout-tracker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_executable(workout-tracker
  src/main.cpp
  src/app/App.cpp
  src/inference/PoseEstimator.cpp
  src/control/MotorController.cpp
)

target_include_directories(workout-tracker PRIVATE include)
target_link_libraries(workout-tracker
  PRIVATE
    pthread
)

# Nice-to-have: put runtime in build/ for run_local.sh
set_target_properties(workout-tracker PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})


target_link_libraries(workout-tracker PRIVATE gpiod)
