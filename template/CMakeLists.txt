include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9
)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 6b5799bae8a1fe3792691ef4b72ae26434c3e799 # latest commit on 19-11-2026
)
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    SOURCE_SUBDIR cmake
)
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1
)
FetchContent_Declare(
    opencl_headers
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-Headers.git
    GIT_TAG v2026.05.29
)
FetchContent_Declare(
    opencl_icd_loader
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-ICD-Loader.git
    GIT_TAG v2026.05.29
)
FetchContent_MakeAvailable(glfw)
FetchContent_MakeAvailable(imgui)
FetchContent_MakeAvailable(stb)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(OPENCL_HEADERS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPENCL_HEADERS_BUILD_CXX_TESTS OFF CACHE BOOL "" FORCE)
set(OPENCL_ICD_LOADER_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_OPENCL_LAYERS OFF CACHE BOOL "" FORCE)
set(ENABLE_OPENCL_LAYERINFO OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glad)
FetchContent_MakeAvailable(zlib)
FetchContent_MakeAvailable(opencl_headers)
FetchContent_MakeAvailable(opencl_icd_loader)
glad_add_library(glad_gl_core_33 REPRODUCIBLE EXCLUDE_FROM_ALL LOADER API gl:core=3.3)

find_package(OpenMP QUIET COMPONENTS CXX)
if(NOT OpenMP_CXX_FOUND)
    message(FATAL_ERROR "OpenMP was not found. Please install it and try again.")
endif()

file(GLOB_RECURSE TEMPLATE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# Keep this explicit so we do not accidentally compile unsupported backends/examples.
set(IMGUI_FILES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui.h
    ${imgui_SOURCE_DIR}/imconfig.h
    ${imgui_SOURCE_DIR}/imgui_internal.h
    ${imgui_SOURCE_DIR}/imstb_rectpack.h
    ${imgui_SOURCE_DIR}/imstb_textedit.h
    ${imgui_SOURCE_DIR}/imstb_truetype.h
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.h
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.h
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3_loader.h
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Template" FILES ${TEMPLATE_FILES})
source_group(TREE "${imgui_SOURCE_DIR}" PREFIX "ImGui" FILES ${IMGUI_FILES})

add_library(tmpl8 STATIC ${TEMPLATE_FILES} ${IMGUI_FILES})
add_library(tmpl8::tmpl8 ALIAS tmpl8)

target_include_directories(tmpl8 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${stb_SOURCE_DIR}
)

target_compile_definitions(tmpl8 PUBLIC
    IMGUI_IMPL_OPENGL_LOADER_GLAD2
    CL_TARGET_OPENCL_VERSION=220
)

if(MSVC)
    target_compile_options(tmpl8 PUBLIC /fp:fast /EHsc /arch:AVX2)
else()
    target_compile_options(tmpl8 PUBLIC -msse4.1)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(tmpl8 PUBLIC -fms-extensions)
    endif()
endif()

target_link_libraries(tmpl8 PUBLIC glfw glad_gl_core_33 zlibstatic OpenCL::OpenCL OpenMP::OpenMP_CXX)

if(WIN32)
    target_link_libraries(tmpl8 PUBLIC winmm advapi32 user32 gdi32 shell32 OpenGL32)
endif()

# tmpl8_copy_assets(<target>)
# Copies the shared assets owned by the template next to the given executable.
function(tmpl8_copy_assets target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../assets"
                "$<TARGET_FILE_DIR:${target}>/assets"
        COMMENT "Copying assets next to executable"
    )
endfunction()
