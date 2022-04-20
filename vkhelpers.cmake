
#-------------------------------------------------------------------------------
# vkhelers
#-------------------------------------------------------------------------------

set(VKHELPERS_SRC
    third/vkhelpers/include/vkh.h
    third/vkhelpers/src/vkh_app.c
    third/vkhelpers/src/vkh_buffer.c
    third/vkhelpers/src/vkh_device.c
    third/vkhelpers/src/vkh_image.c
    third/vkhelpers/src/vkh_phyinfo.c
    third/vkhelpers/src/vkh_presenter.c
    third/vkhelpers/src/vkh_queue.c
    third/vkhelpers/src/vkhelpers.c
    third/vkhelpers/src/VmaUsage.cpp
    third/vkhelpers/src/deps/tinycthread.h
    third/vkhelpers/src/deps/tinycthread.c)

add_library(vkhelpers STATIC ${VKHELPERS_SRC})
target_include_directories(vkhelpers SYSTEM
    PUBLIC  third/vkhelpers/src/deps
            third/vkhelpers/include
            third/vkhelpers/src
            ${Vulkan_INCLUDE_DIRS})
target_link_libraries(vkhelpers PUBLIC ${Vulkan_LIBRARIES})

target_compile_definitions(vkhelpers PRIVATE
    USE_VMA
    VKH_USE_VALIDATION)

