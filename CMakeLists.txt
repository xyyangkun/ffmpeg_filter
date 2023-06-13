project(ffmpegtest)
cmake_minimum_required(VERSION 3.1.3)
#使能c++11
set(CMAKE_CXX_STANDARD 11)
#加载自定义模块
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_BUILD_TYPE "Debug")
#set (CMAKE_BUILD_TYPE "Release")

if(${CMAKE_BUILD_TYPE} MATCHES "Release")
    message(STATUS "Release版本")
    set(BuildType "Release")
else()
    set(BuildType "Debug")
    message(STATUS "Debug版本")
	#add debug operation
	set(ENABLE_ASAN true)
endif()

if(ENABLE_ASAN)
# 以下问内存泄漏检测
add_compile_options(-g)
add_compile_options(-fsanitize=address)
add_compile_options(-fno-omit-frame-pointer)
add_compile_options(-Wl,--no-undefined)
add_compile_options(-O0)
#添加链接选项, 只能添加一行
SET(CMAKE_EXE_LINKER_FLAGS "-fsanitize=address")
endif()

# 忽略deprecated-declarations 警告与错误
add_compile_options(-Wno-deprecated-declarations)
add_compile_options(-Wno-error=deprecated-declarations)

#设置bin和lib库目录
set(RELEASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/release)
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    SET(LIBRARY_OUTPUT_PATH ${RELEASE_DIR}/linux/${BuildType})
    SET(EXECUTABLE_OUTPUT_PATH ${RELEASE_DIR}/linux/${BuildType})
    add_compile_options(-fPIC)
elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
    SET(LIBRARY_OUTPUT_PATH ${RELEASE_DIR}/windows/${BuildType})
    SET(EXECUTABLE_OUTPUT_PATH ${RELEASE_DIR}/windows/${BuildType})
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    SET(LIBRARY_OUTPUT_PATH ${RELEASE_DIR}/mac/${BuildType})
    SET(EXECUTABLE_OUTPUT_PATH ${RELEASE_DIR}/mac/${BuildType})
endif ()

LINK_DIRECTORIES(${LIBRARY_OUTPUT_PATH})


ADD_DEFINITIONS(-DARM)
IF(CROSS_COMPILE)
# 设置默认链接块
set(LINK_LIB_LIST m pthread asound avformat avdevice avcodec avfilter swresample avutil rga drm rockchip_mpp  z asound v4l2 v4lconvert jpeg)
message(STATUS "arm corss compile")
#add_definitions(-DARM)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DARM")

ELSE()
set(LINK_LIB_LIST m pthread asound swresample swscale asound avformat avdevice avcodec avfilter swresample avutil )
message(STATUS "x86 compile")
ENDIF(CROSS_COMPILE)

IF(CROSS_COMPILE)
set(SDL2_INCLUDE_DIR   ${SYSROOT_PATH}/usr/include)
set(SDL2_LIBRARIES "-L ${SYSROOT_PATH}/usr/lib -lSDL2")
ELSE()
find_package(SDL2 QUIET)
ENDIF(CROSS_COMPILE)
if (SDL2_FOUND)
    include_directories(${SDL2_INCLUDE_DIR})
    list(APPEND LINK_LIB_LIST ${SDL2_LIBRARY})
    message(STATUS "found library:${SDL2_LIBRARY}")
else()
    message(STATUS "not found library:SDL2_LIBRARY")
endif (SDL2_FOUND)




IF(CROSS_COMPILE)
	set(AVUTIL_INCLUDE_DIR   ${SYSROOT_PATH}/usr/include)
	set(AVUTIL_LIBRARIES "-L ${SYSROOT_PATH}/usr/lib -lavutil")
	set(AVUTIL_FOUND TRUE)
ELSE()
    find_package(AVUTIL QUIET)
ENDIF(CROSS_COMPILE)
if(AVUTIL_FOUND)
	include_directories(${AVUTIL_INCLUDE_DIR})
	list(APPEND  LINK_LIB_LIST ${AVUTIL_LIBRARIES})
	message(STATUS "found library:${AVUTIL_LIBRARIES}")
else()
	message(STATUS "not found library:AVUTIL_LIBRARIES")
endif()


#查找ffmpeg/libavcodec是否安装
IF(CROSS_COMPILE)
	set(AVCODEC_INCLUDE_DIR   ${SYSROOT_PATH}/usr/include)
	set(_LIBRARIES "-L ${SYSROOT_PATH}/usr/lib -lavcodec")
	set(AVCODEC_FOUND TRUE)
ELSE()
    find_package(AVCODEC QUIET)
ENDIF(CROSS_COMPILE)
if(AVCODEC_FOUND)
	include_directories(${AVCODEC_INCLUDE_DIR})
	list(APPEND  LINK_LIB_LIST ${AVCODEC_LIBRARIES})
	message(STATUS "found library:${AVCODEC_LIBRARIES}")
else()
	message(STATUS "not found library:AVCODEC_LIBRARIES")
endif()

#查找ffmpeg/libavfilter 是否安装
IF(CROSS_COMPILE)
	set(AVFILTER_INCLUDE_DIR   ${SYSROOT_PATH}/usr/include)
	set(AVFILTER_LIBRARIES "-L ${SYSROOT_PATH}/usr/lib -lavfilter")
	set(AVFILTER_FOUND 1)
ELSE()
	find_package(AVFILTER QUIET)
ENDIF(CROSS_COMPILE)
if(AVFILTER_FOUND)
	include_directories(${AVFILTER_INCLUDE_DIR})
	list(APPEND  LINK_LIB_LIST ${AVFILTER_LIBRARIES})
	message(STATUS "found library:${AVFILTER_LIBRARIES}")
else()
	message(STATUS "not found library:AVFILTER_LIBRARIES")
endif()

#查找ffmpeg/libavformat 是否安装
IF(CROSS_COMPILE)
	set(AVFORMAT_INCLUDE_DIR   ${SYSROOT_PATH}/usr/include)
	set(AVFORMAT_LIBRARIES "-L ${SYSROOT_PATH}/usr/lib -lavformat")
	set(AVFORMAT_FOUND 1)
ELSE()
	find_package(AVFORMAT QUIET)
ENDIF(CROSS_COMPILE)
if(AVFORMAT_FOUND)
	include_directories(${AVFORMAT_INCLUDE_DIR})
	list(APPEND  LINK_LIB_LIST ${AVFORMAT_LIBRARIES})
	message(STATUS "found library:${AVFORMAT_LIBRARIES}")
else()
	message(STATUS "not found library:AVFORMAT_LIBRARIES")
endif()

#查找ffmpeg/libavdevice 是否安装
IF(CROSS_COMPILE)
	set(AVDEVICE_INCLUDE_DIR   ${SYSROOT_PATH}/usr/include)
	set(AVDEVICE_LIBRARIES "-L ${SYSROOT_PATH}/usr/lib -lavdevice")
	set(AVDEVICE_FOUND 1)
ELSE()
	find_package(AVDEVICE QUIET)
ENDIF(CROSS_COMPILE)
if(AVDEVICE_FOUND)
	include_directories(${AVDEVICE_INCLUDE_DIR})
	list(APPEND  LINK_LIB_LIST ${AVDEVICE_LIBRARIES})
	message(STATUS "found library:${AVDEVICE_LIBRARIES}")
else()
	message(STATUS "not found library:AVDEVICE_LIBRARIES")
endif()






aux_source_directory(. TEST_SRC_LIST)
#如果ffmpeg/libavcodec ffmpeg/libavcodec SDL 都安装了则编译 test_player
if(SDL2_FOUND AND AVCODEC_FOUND AND AVUTIL_FOUND)
else()
    message(STATUS "test_player ingored, please install sdl2 ffmpeg/libavcodec ffmpeg/libavutil")
    list(REMOVE_ITEM TEST_SRC_LIST ./test_player.cpp)
    list(REMOVE_ITEM TEST_SRC_LIST ./test_player.cpp)
endif()


#list(REMOVE_ITEM TEST_SRC ./sound_card_get.c ./sound_card_get_test.c)
list(REMOVE_ITEM TEST_SRC_LIST ./sound_card_get.c ./sound_card_get_test.c ./multi_audio_mix.c ./multi_audio_mix_48kin.c ./proc_cmd.cpp  ./t_proc_cmd.cpp ./proc_mix_cmd.cpp ./test_demux.c ./demux.c 
								./test_demux_audio_player.c ./test_sdl_timer.c ./test_demux_av_player.c ./audio_recv.c ./five_audio_mix_48kin.c ./audio_play.c ./audio_card_recv.c
								./test_audio_recv_play.c ./test_wav_play.c ./wavreader.c
								./test_hdmi_audio_recv_play.c ./hdmi_audio_card_recv.c ./hdmi_audio_recv.h)

foreach(TEST_SRC ${TEST_SRC_LIST})
    STRING(REGEX REPLACE "^\\./|\\.c[a-zA-Z0-9_]*$" "" TEST_EXE_NAME ${TEST_SRC})
    message(STATUS "add test:${TEST_EXE_NAME}")
    add_executable(${TEST_EXE_NAME} ${TEST_SRC})

	if(WIN32)
		set_target_properties(${TEST_EXE_NAME} PROPERTIES COMPILE_FLAGS ${VS_FALGS} )
	endif(WIN32)
    target_link_libraries(${TEST_EXE_NAME} ${LINK_LIB_LIST})
endforeach()


if(MSVC AND SDL2_FOUND AND AVCODEC_FOUND AND AVUTIL_FOUND)
    set_target_properties(test_player PROPERTIES LINK_FLAGS "/SAFESEH:NO /SUBSYSTEM:WINDOWS" )
endif()


file(GLOB sound_card_get_test_src_list sound_card_get.c sound_card_get_test.c)
add_executable(sound_card_get_test  ${sound_card_get_test_src_list})  
target_link_libraries(sound_card_get_test ${LINK_LIB_LIST})

file(GLOB multi_audio_mix_list multi_audio_mix.c sound_card_get.c)
add_executable(multi_audio_mix ${multi_audio_mix_list})  
target_link_libraries(multi_audio_mix ${LINK_LIB_LIST})

file(GLOB multi_audio_mix_48kin_list multi_audio_mix_48kin.c sound_card_get.c)
add_executable(multi_audio_mix_48kin ${multi_audio_mix_48kin_list})  
target_link_libraries(multi_audio_mix_48kin ${LINK_LIB_LIST})
#编译目标设置-DTEST宏
#https://stackoverflow.com/questions/5096881/does-set-target-properties-in-cmake-override-cmake-cxx-flags
set_target_properties(multi_audio_mix_48kin PROPERTIES  COMPILE_FLAGS "-DTEST") # ok
#target_compile_definitions(multi_audio_mix_48kin PRIVATE -DTEST)               #ok


file(GLOB five_audio_mix_48kin_list five_audio_mix_48kin.c sound_card_get.c audio_recv.c audio_play.c audio_card_recv.c )
add_executable(five_audio_mix_48kin ${five_audio_mix_48kin_list})  
target_link_libraries(five_audio_mix_48kin ${LINK_LIB_LIST})
#编译目标设置-DTEST宏
#https://stackoverflow.com/questions/5096881/does-set-target-properties-in-cmake-override-cmake-cxx-flags
set_target_properties(five_audio_mix_48kin PROPERTIES  COMPILE_FLAGS "-DTEST") # ok
#target_compile_definitions(five_audio_mix_48kin PRIVATE -DTEST)               #ok





#file(GLOB proc_cmd_list proc_cmd.cpp  t_proc_cmd.cpp)
#add_executable(t_proc_cmd ${proc_cmd_list})  
#target_link_libraries(t_proc_cmd ${LINK_LIB_LIST})

# 
file(GLOB proc_mix_cmd_list proc_mix_cmd.cpp  proc_cmd.cpp multi_audio_mix_48kin.c sound_card_get.c)
add_executable(proc_mix_cmd ${proc_mix_cmd_list})  
target_link_libraries(proc_mix_cmd ${LINK_LIB_LIST})

file(GLOB test_demux_list test_demux.c demux.c)
add_executable(test_demux ${test_demux_list})  
target_link_libraries(test_demux ${LINK_LIB_LIST})

file(GLOB test_audio_recv_play_list test_audio_recv_play.c audio_recv.c audio_play.c audio_card_recv.c sound_card_get.c)
add_executable(test_audio_recv_play ${test_audio_recv_play_list})  
target_link_libraries(test_audio_recv_play ${LINK_LIB_LIST})


file(GLOB test_hdmi_audio_recv_play_list test_hdmi_audio_recv_play.c hdmi_audio_card_recv.c audio_play.c audio_card_recv.c sound_card_get.c audio_recv.c)
add_executable(test_hdmi_audio_recv_play ${test_hdmi_audio_recv_play_list})  
target_link_libraries(test_hdmi_audio_recv_play ${LINK_LIB_LIST})

file(GLOB test_wav_play_list test_wav_play.c audio_recv.c audio_play.c audio_card_recv.c sound_card_get.c wavreader.c)
add_executable(test_wav_play ${test_wav_play_list})  
target_link_libraries(test_wav_play ${LINK_LIB_LIST})

file(GLOB test_demux_audio_player_list test_demux_audio_player.c demux.c)
add_executable(test_demux_audio_player ${test_demux_audio_player_list})  
#set_target_properties(test_demux_audio_player PROPERTIES  COMPILE_FLAGS "-DARM") # ok
target_link_libraries(test_demux_audio_player ${LINK_LIB_LIST})

file(GLOB test_demux_av_player_list test_demux_av_player.c demux.c)
add_executable(test_demux_av_player ${test_demux_av_player_list})  
#set_target_properties(test_demux_av_player PROPERTIES  COMPILE_FLAGS "-DARM") # ok
target_link_libraries(test_demux_av_player ${LINK_LIB_LIST})

file(GLOB test_sdl_timer_list test_sdl_timer.c)
add_executable(test_sdl_timer ${test_sdl_timer_list})  
target_link_libraries(test_sdl_timer ${LINK_LIB_LIST})


# alsa读取音频，v4l2读取视频，并且使用ffmpeg编码，推流
file(GLOB av_read_enc_list av_read_enc.c)
add_executable(test_av_read_enc ${av_read_enc_list})  
target_link_libraries(test_av_read_enc ${LINK_LIB_LIST})


add_subdirectory(test_ffmpeg)
