cmake_minimum_required(VERSION 3.24)
project(lampe_beat_benchmark LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# libsamplerate 0.2.2 predates CMake 4's minimum policy version.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(LIBSAMPLERATE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBSAMPLERATE_INSTALL OFF CACHE BOOL "" FORCE)
include(FetchContent)
FetchContent_Declare(samplerate
  GIT_REPOSITORY https://github.com/libsndfile/libsamplerate.git
  GIT_TAG c96f5e3de9c4488f4e6c97f59f5245f22fda22f7)
FetchContent_Declare(btrack
  GIT_REPOSITORY https://github.com/adamstark/BTrack.git
  GIT_TAG 9d6127618a5679e9caa74c594b88f1d74f0e035f
  SOURCE_SUBDIR unused-upstream-build)
FetchContent_MakeAvailable(samplerate btrack)
function(check_revision directory expected)
  execute_process(COMMAND git -C ${directory} rev-parse HEAD OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
  if(NOT revision STREQUAL expected)
    message(FATAL_ERROR "Benchmark dependency revision mismatch: ${directory}")
  endif()
  execute_process(COMMAND git -C ${directory} diff --quiet HEAD RESULT_VARIABLE dirty)
  if(NOT dirty EQUAL 0)
    message(FATAL_ERROR "Benchmark requires unmodified dependency sources: ${directory}")
  endif()
endfunction()
check_revision(${btrack_SOURCE_DIR} 9d6127618a5679e9caa74c594b88f1d74f0e035f)
check_revision(${samplerate_SOURCE_DIR} c96f5e3de9c4488f4e6c97f59f5245f22fda22f7)

# Compile the unchanged upstream implementation, using its bundled Kiss FFT.
add_library(btrack_reference STATIC
  ${btrack_SOURCE_DIR}/src/BTrack.cpp
  ${btrack_SOURCE_DIR}/src/OnsetDetectionFunction.cpp
  ${btrack_SOURCE_DIR}/libs/kiss_fft130/kiss_fft.c)
target_include_directories(btrack_reference SYSTEM PUBLIC
  ${btrack_SOURCE_DIR}/src ${btrack_SOURCE_DIR}/libs/kiss_fft130)
target_compile_definitions(btrack_reference PUBLIC USE_KISS_FFT)
target_link_libraries(btrack_reference PRIVATE samplerate)

add_executable(beat_benchmark replay.cpp)
target_include_directories(beat_benchmark PRIVATE ../../lib/LampLogic)
target_link_libraries(beat_benchmark PRIVATE btrack_reference)
target_compile_options(beat_benchmark PRIVATE -Wall -Wextra -Werror -pedantic)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/LampLogic/BeatTracker.h)
file(SHA256 ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/LampLogic/BeatTracker.h LAMP_HEADER_SHA256)
target_compile_definitions(beat_benchmark PRIVATE LAMP_HEADER_SHA256="${LAMP_HEADER_SHA256}")
add_executable(audio_features audio_features.cpp)
target_link_libraries(audio_features PRIVATE btrack_reference)
target_compile_options(audio_features PRIVATE -Wall -Wextra -Werror -pedantic)
add_executable(lamp_replay ../../simulator/beat_replay.cpp)
target_include_directories(lamp_replay PRIVATE ../../lib/LampLogic)
