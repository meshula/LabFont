
cmake_minimum_required(VERSION 3.19)
project(meshula-font C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(meshula_font_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# Don't report that sample file installation up to date
set(CMAKE_INSTALL_MESSAGE LAZY)

if(APPLE)
    set(ST_GFX_DEFS
        SOKOL_METAL
)
else()
    set(ST_GFX_DEFS
        SOKOL_D3D11
    )
endif()

include(sokol.cmake)

configure_file("${meshula_font_ROOT}/LabFontDemo.config.h"
               "${meshula_font_ROOT}/LabFontDemo.h" @ONLY)

set(SRC
    src/fontstash.h
    src/lab_sokol.h
    src/LabFont.cpp
    src/LabFontDemo.cpp
    src/lab_zep.cpp
    src/lab_microui.h
    src/lab_microui.cpp
    src/main.cpp
    src/microui.c
    src/microui.h
    src/microui_demo.cpp
    src/microui_demo.h
    src/quadplay_font.cpp
    src/stb_image.h
    src/stb_truetype.h
    src/lab_floooh_8bitfont.cpp
    src/lab_floooh_8bitfont.h
    third/zep/extensions/repl/mode_repl.cpp
    third/zep/extensions/repl/mode_repl.h
    LabFont.h
    LabFontDemo.h
    LabZep.h
    #src/VideoReader.c
    #src/VideoReader.h
)

if (APPLE)
    set(PLATFORM_SRC src/mac_menus.m)
endif()

set(BUILD_IMGUI OFF)
set(BUILD_DEMOS OFF)
set(BUILD_TESTS OFF)
add_subdirectory(third/zep)

add_executable(meshula-font MACOSX_BUNDLE ${SRC} ${PLATFORM_SRC})

if (APPLE)
  #  list(APPEND LIBS
  #      "-framework OpenGL"
  #  )

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/MacBundleInfo.plist.in
        ${CMAKE_CURRENT_BINARY_DIR}/resources/MacBundleInfo.plist)

    set_target_properties(meshula-font PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/resources/MacBundleInfo.plist)

    target_compile_definitions(meshula-font PRIVATE
        ${ST_GFX_DEFS}
        ${PLATFORM_DEFS}
    )

elseif(WIN32)
#    list(APPEND LIBS
#        "-lopengl32"
#    )

    target_compile_definitions(meshula-font PRIVATE
        ${ST_GFX_DEFS}
        ${PLATFORM_DEFS}
        _CRT_SECURE_NO_WARNINGS
    )
else()
    list(APPEND LIBS
        "-lGL -lX11"
    )
endif()

target_include_directories(meshula-font PRIVATE
    third
    third/rapidjson/include
    third/zep/include)

if (WIN32)
    target_link_libraries(meshula-font PRIVATE
        ${LIBS} sokol::sokol Zep::Zep)
    #    "-lglfw" "-lavcodec" "-lavformat" "-lswscale" ${LIBS})
else()
    target_link_libraries(meshula-font PRIVATE
        "-lstdc++" ${LIBS} sokol::sokol Zep::Zep)
    #    "-lglfw" "-lavcodec" "-lavformat" "-lswscale" ${LIBS})
endif()

install(TARGETS meshula-font 
         RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
         BUNDLE  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(FILES
    "${meshula_font_ROOT}/resources/DroidSansJapanese.ttf"
    "${meshula_font_ROOT}/resources/DroidSerif-Bold.ttf"
    "${meshula_font_ROOT}/resources/DroidSerif-Italic.ttf"
    "${meshula_font_ROOT}/resources/DroidSerif-Regular.ttf"
    "${meshula_font_ROOT}/resources/robot-18.png"
    "${meshula_font_ROOT}/resources/robot-18.font.json"
    "${meshula_font_ROOT}/resources/hauer-12.png"
    "${meshula_font_ROOT}/resources/hauer-12.font.json"
    "${meshula_font_ROOT}/third/zep/demos/demo_imgui/res/Cousine-Regular.ttf"
    DESTINATION share/lab_font_demo)
