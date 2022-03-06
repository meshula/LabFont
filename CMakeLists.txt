
cmake_minimum_required(VERSION 3.19)
project(LabFont C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(LABFONT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
option(LABFONT_SOKOL "Use Sokol as the renderer" OFF)
option(LABFONT_METAL "Use Metal as the renderer" ON)

# Don't report that sample file installation up to date
set(CMAKE_INSTALL_MESSAGE LAZY)

if (LABFONT_SOKOL AND LABFONT_METAL)
    message(FATAL "Cannot select Sokol and Metal simultaneously")
endif()

if (LABFONT_SOKOL)
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

    set(LABFONT_PLATFORM_SRC
        src/main-sokol.cpp
        src/LabFont-sokol.cpp
        src/lab_microui-sokol.cpp)

    set(LABFONT_PLATFORM_LIBS sokol::sokol)

    list(APPEND ST_GFX_DEFS LABFONT_HAVE_SOKOL)
endif()

if (LABFONT_METAL)
    set(LABFONT_PLATFORM_SRC
        src/main-metal.mm
        src/LabFont-metal.mm
        src/lab_microui-metal.mm
        src/mtlfontstash.h)
    set(ST_GFX_DEFS LABFONT_HAVE_METAL)
endif()

set(SRC
    LabDrawImmediate.h
    LabDrawImmediate-metal.h
    LabFont.h
    LabSokol.h
    LabZep.h
    ${LABFONT_PLATFORM_SRC}
    src/fontstash.h
    src/LabDirectories.cpp
    src/LabDirectories.h
    src/LabFontDemo.cpp
    src/lab_zep.cpp
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

add_executable(LabFontDemo ${SRC} ${PLATFORM_SRC})
set_property(TARGET LabFontDemo PROPERTY CXX_STANDARD 17)

if (APPLE)
    list(APPEND LIBS
        "-framework Cocoa"
        "-framework Metal"
        "-framework QuartzCore"
    )

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/MacBundleInfo.plist.in
        ${CMAKE_CURRENT_BINARY_DIR}/resources/MacBundleInfo.plist)

    set_target_properties(LabFontDemo PROPERTIES
        BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/resources/MacBundleInfo.plist)

    target_compile_definitions(LabFontDemo PRIVATE
        ${ST_GFX_DEFS}
        ${PLATFORM_DEFS}
    )

elseif(WIN32)
#    list(APPEND LIBS
#        "-lopengl32"
#    )

    target_compile_definitions(LabFontDemo PRIVATE
        ${ST_GFX_DEFS}
        ${PLATFORM_DEFS}
        _CRT_SECURE_NO_WARNINGS
    )
else()
    list(APPEND LIBS
        "-lGL -lX11"
    )
endif()

target_include_directories(LabFontDemo PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    src
    third
    third/rapidjson/include
    third/zep/include)

if (WIN32)
    target_link_libraries(LabFontDemo PRIVATE
        ${LIBS} ${LABFONT_PLATFORM_LIBS} Zep::Zep)
    #    "-lglfw" "-lavcodec" "-lavformat" "-lswscale" ${LIBS})
else()
    target_link_libraries(LabFontDemo PRIVATE
        "-lstdc++" ${LIBS} ${LABFONT_PLATFORM_LIBS} Zep::Zep)
    #    "-lglfw" "-lavcodec" "-lavformat" "-lswscale" ${LIBS})
endif()

install(TARGETS LabFontDemo 
         RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
         BUNDLE  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

set(LABFONT_RESOURCES
    "${LABFONT_ROOT}/resources/DroidSansJapanese.ttf"
    "${LABFONT_ROOT}/resources/DroidSerif-Bold.ttf"
    "${LABFONT_ROOT}/resources/DroidSerif-Italic.ttf"
    "${LABFONT_ROOT}/resources/DroidSerif-Regular.ttf"
    "${LABFONT_ROOT}/resources/robot-18.png"
    "${LABFONT_ROOT}/resources/robot-18.font.json"
    "${LABFONT_ROOT}/resources/hauer-12.png"
    "${LABFONT_ROOT}/resources/hauer-12.font.json"
    "${LABFONT_ROOT}/resources/Cousine-Regular.ttf")

install(FILES ${LABFONT_RESOURCES}
    DESTINATION share/lab_font_demo)

