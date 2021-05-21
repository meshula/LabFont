
#-------------------------------------------------------------------------------
# sokol
#-------------------------------------------------------------------------------

set(SOKOL_SRC 
    src/sokol.c
    src/sokol.cpp)

set(SOKOL_HEADERS 
    third/sokol/sokol_app.h
    third/sokol/sokol_args.h
    third/sokol/sokol_audio.h
    third/sokol/sokol_fetch.h
    third/sokol/sokol_gfx.h
    third/sokol/sokol_time.h
    third/sokol/util/sokol_fontstash.h
#    third/sokol/util/sokol_gfx_imgui.h
    third/sokol/util/sokol_gl.h
#    third/sokol/util/sokol_imgui.h
)
add_library(sokol STATIC ${SOKOL_SRC} ${SOKOL_HEADERS})
target_include_directories(sokol SYSTEM 
    PUBLIC third/sokol third/sokol/util src)
#target_link_libraries(sokol imgui)
#add_dependencies(sokol imgui)
target_compile_definitions(sokol PRIVATE
    ${ST_GFX_DEFS}
    #IMGUI_DEFINE_MATH_OPERATORS
    #HAVE_IMGUI
    ${PLATFORM_DEFS}
)

if (WIN32)
    set(PLATFORM_LIBS ws2_32 Iphlpapi.lib opengl32.lib)
elseif (APPLE)
#    if(CMAKE_OSX_SYSROOT MATCHES ".*iphoneos.*")
#        set(DARWIN_LIBS
#            "-framework AudioToolbox"
#            "-framework Accelerate"
#            "-framework CoreAudio")
#    else()
        set(PLATFORM_LIBS
            #"-framework AudioToolbox"
            #"-framework AudioUnit"
            #"-framework Accelerate"
            "-framework Cocoa"
            #"-framework CoreAudio"
            "-framework Metal"
            "-framework MetalKit"
            "-framework QuartzCore"
            )
#    endif()
endif()

target_link_libraries(sokol PUBLIC ${PLATFORM_LIBS})

if(APPLE)
    set_source_files_properties(src/sokol.cpp PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-arc")
endif()

install(
    TARGETS sokol
    EXPORT pxrTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/sokol
)

add_library(sokol::sokol ALIAS sokol)
