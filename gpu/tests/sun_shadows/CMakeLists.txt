cmake_minimum_required(VERSION 3.24)
project(sun_shadow_contracts LANGUAGES CXX)
enable_testing()
find_package(Threads REQUIRED)

# Build the same policy headers consumed by the renderer without a GPU or game data.
foreach(test atlas_contracts clip_contracts coherence depth_policy memo_contracts owner_gate sync_contracts)
    add_executable(${test} ${test}.cpp)
    target_compile_features(${test} PRIVATE cxx_std_23)
    target_include_directories(${test} PRIVATE ../../shadps4/video_core/renderer_vulkan)
    target_link_libraries(${test} PRIVATE Threads::Threads)
    # Contract checks remain enabled for optimized test builds.
    if(MSVC)
        target_compile_options(${test} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${test} PRIVATE -UNDEBUG)
    endif()
    add_test(NAME ${test} COMMAND ${test})
endforeach()
