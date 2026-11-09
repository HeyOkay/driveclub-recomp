cmake_minimum_required(VERSION 3.24)
project(AcceptedMainPortable LANGUAGES C CXX)
set(MAIN_CLANG64_ROOT "C:/msys64/clang64" CACHE PATH "Installed MSYS2 CLANG64 dependency prefix")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BB_DIAGNOSTICS OFF CACHE BOOL "" FORCE)
set(BB_LTO ON CACHE BOOL "" FORCE)
set(BB_PGO use CACHE STRING "PGO mode: use, off, generate")
set(BB_PGO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/host/pgo" CACHE PATH "Profile directory")
set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "" FORCE)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/profile-source-manifest.json")
  message(FATAL_ERROR "Assemble the working-main source profile first")
endif()
add_subdirectory(host/gpu accepted-host)
# The accepted C compilation database explicitly kept stale PGO counts as warnings.
# Preserve -Werror for source diagnostics while matching that recorded exception.
target_compile_options(DriveClubGame PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-error=backend-plugin>)
include(AcceptedSources.cmake)
find_program(MAIN_GLSLANG NAMES glslangValidator glslang REQUIRED)
find_program(MAIN_SPIRV_VAL NAMES spirv-val REQUIRED)
set(MAIN_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
set(MAIN_SHADER_DIR "${MAIN_GENERATED}/video_core/host_shaders")
set(MAIN_HELPER_HEADERS)
# Keep default portable helpers for the generic transform overload, plus all
# runtime variants. No captured/native shader body is a source input.
set(MAIN_HELPER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/portable/host_shaders")
set(MAIN_FAR_POLICY "${MAIN_HELPER_SOURCE_DIR}/sun_far_filter_policy.glsl")
foreach(kind portable standard high ultra)
  foreach(stage vertex fragment)
    if(stage STREQUAL "vertex")
      set(extension vert)
    else()
      set(extension frag)
    endif()
    string(TOUPPER ${kind} kind_upper)
    string(TOUPPER ${stage} stage_upper)
    set(src "${MAIN_HELPER_SOURCE_DIR}/sun_${kind}_${stage}.${extension}")
    set(header "${MAIN_SHADER_DIR}/sun_${kind}_${stage}.h")
    set(binary "${MAIN_SHADER_DIR}/sun_${kind}_${stage}.spv")
    add_custom_command(OUTPUT "${header}" "${binary}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${MAIN_SHADER_DIR}"
      COMMAND ${MAIN_GLSLANG} "-I${MAIN_HELPER_SOURCE_DIR}" --target-env vulkan1.3
        --vn SUN_${kind_upper}_${stage_upper} -o "${header}" "${src}"
      COMMAND ${MAIN_GLSLANG} "-I${MAIN_HELPER_SOURCE_DIR}" -V --target-env vulkan1.3
        -o "${binary}" "${src}"
      COMMAND ${MAIN_SPIRV_VAL} --target-env vulkan1.3 "${binary}"
      DEPENDS "${src}" "${MAIN_FAR_POLICY}" VERBATIM)
    list(APPEND MAIN_HELPER_HEADERS "${header}" "${binary}")
  endforeach()
endforeach()
add_custom_target(main_sun_helpers DEPENDS ${MAIN_HELPER_HEADERS})
add_dependencies(bbgpu main_sun_helpers)
target_include_directories(bbgpu BEFORE PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/portable/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/portable/settings"
  "${CMAKE_CURRENT_SOURCE_DIR}/portable"
  "${MAIN_GENERATED}")
target_sources(bbgpu PRIVATE
  portable/main_sun_portable.cpp portable/main_standard_filter.cpp portable/main_quality_filter.cpp
  portable/sun_shader_transform.cpp portable/sun_shader_link.cpp)
foreach(lib SPIRV-Tools-link SPIRV-Tools-opt SPIRV-Tools)
  string(REPLACE "-" "_" key ${lib})
  find_file(MAIN_${key}_STATIC NAMES "lib${lib}.a" PATHS "${MAIN_CLANG64_ROOT}/lib" REQUIRED NO_DEFAULT_PATH)
  target_link_libraries(bbgpu PRIVATE "${MAIN_${key}_STATIC}")
endforeach()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
# Finalize distributed runtime bytes BEFORE compiling the backend identity gate.
option(MAIN_STRIP_RUNTIME "Strip the distributed runtime before hashing" ON)
find_program(MAIN_LLVM_STRIP NAMES llvm-strip REQUIRED)
set(MAIN_FINAL_RUNTIME "${CMAKE_CURRENT_BINARY_DIR}/finalized-runtime/DriveClubGame.exe")
add_custom_command(OUTPUT "${MAIN_FINAL_RUNTIME}"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/finalize_runtime.py"
    "$<TARGET_FILE:DriveClubGame>" "${MAIN_FINAL_RUNTIME}"
    "${MAIN_STRIP_RUNTIME}" "${MAIN_LLVM_STRIP}"
  DEPENDS DriveClubGame "${CMAKE_CURRENT_SOURCE_DIR}/finalize_runtime.py" VERBATIM)
set(MAIN_IDENTITY "${CMAKE_CURRENT_BINARY_DIR}/generated/launcher/main_runtime_identity.generated.h")
add_custom_command(OUTPUT "${MAIN_IDENTITY}"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/generate_runtime_identity.py"
          "${MAIN_FINAL_RUNTIME}" "${MAIN_IDENTITY}"
  DEPENDS "${MAIN_FINAL_RUNTIME}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_runtime_identity.py" VERBATIM)
set(MAIN_LAUNCHER "${CMAKE_CURRENT_SOURCE_DIR}/host/launcher")
add_executable(DriveClubBackend EXCLUDE_FROM_ALL
  "${MAIN_IDENTITY}"
  "${MAIN_LAUNCHER}/driveclub_launcher.cpp" "${MAIN_LAUNCHER}/prepare.cpp"
  "${MAIN_LAUNCHER}/gpu_check.cpp" "${MAIN_LAUNCHER}/datx.cpp"
  "${MAIN_LAUNCHER}/unlock.cpp" "${MAIN_LAUNCHER}/graphics.cpp"
  "${MAIN_LAUNCHER}/sun_shadows.cpp" "${MAIN_LAUNCHER}/carswitch.cpp"
  "${MAIN_LAUNCHER}/updater.cpp" "${MAIN_LAUNCHER}/launcher.rc")
set_target_properties(DriveClubBackend PROPERTIES OUTPUT_NAME "DriveClub.Backend" CXX_STANDARD 20 INTERPROCEDURAL_OPTIMIZATION OFF)
target_compile_definitions(DriveClubBackend PRIVATE MINIZ_STATIC_DEFINE)
target_include_directories(DriveClubBackend PRIVATE "${MAIN_LAUNCHER}"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/launcher"
  "${CMAKE_CURRENT_SOURCE_DIR}/host/gpu/third_party/miniz"
  "${CMAKE_CURRENT_BINARY_DIR}/accepted-host/third_party/miniz")
target_compile_options(DriveClubBackend PRIVATE -O2 -municode)
target_link_options(DriveClubBackend PRIVATE -static -municode -mwindows)
target_link_libraries(DriveClubBackend PRIVATE miniz::miniz gdiplus gdi32 user32 shell32 ole32 dwmapi winhttp bcrypt comctl32 oleaut32 wbemuuid iphlpapi)
add_custom_command(TARGET DriveClubBackend POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:DriveClubBackend>" "$<TARGET_FILE_DIR:DriveClubBackend>/DriveClub.Native.exe")

set(MAIN_REAR_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/host/launcher/rear_native_payload.h")
add_custom_command(OUTPUT "${MAIN_REAR_HEADER}"
 COMMAND ${Python3_EXECUTABLE} "${MAIN_LAUNCHER}/tools/generate_rear_payload.py"
  --clang "${CMAKE_C_COMPILER}" --output "${MAIN_REAR_HEADER}"
 DEPENDS "${MAIN_LAUNCHER}/tools/generate_rear_payload.py" "${MAIN_LAUNCHER}/tools/rear-native-stub.s" VERBATIM)
add_custom_target(main_rear_shim DEPENDS "${MAIN_REAR_HEADER}")
add_dependencies(DriveClubBackend main_rear_shim)
