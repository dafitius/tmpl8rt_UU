set(PROJECT_NAME Basics)

file(GLOB_RECURSE PROJECT_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${PROJECT_FILES})

add_executable(${PROJECT_NAME} ${PROJECT_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE tmpl8::tmpl8)

if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()

tmpl8_copy_assets(${PROJECT_NAME})
