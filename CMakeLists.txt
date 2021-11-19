project(ffmpegtest)
cmake_minimum_required(VERSION 3.1.3)
#使能c++11
set(CMAKE_CXX_STANDARD 11)
#加载自定义模块
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

#set(CMAKE_BUILD_TYPE "Debug")

if(${CMAKE_BUILD_TYPE} MATCHES "Release")
    message(STATUS "Release版本")
    set(BuildType "Release")
else()
    set(BuildType "Debug")
    message(STATUS "Debug版本")
endif()

#add debug operation
set(ENABLE_ASAN true)

if(ENABLE_ASAN)
# 以下问内存泄漏检测
add_compile_options(-g)
add_compile_options(-fsanitize=address)
add_compile_options(-fno-omit-frame-pointer)
add_compile_options(-Wl,--no-undefined)
#add_compile_options(-O1)
#添加链接选项, 只能添加一行
SET(CMAKE_EXE_LINKER_FLAGS "-fsanitize=address")
endif()

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


# 设置默认链接块
set(LINK_LIB_LIST m pthread)

find_package(SDL2 QUIET)
if (SDL2_FOUND)
    include_directories(${SDL2_INCLUDE_DIR})
    list(APPEND LINK_LIB_LIST ${SDL2_LIBRARY})
    message(STATUS "found library:${SDL2_LIBRARY}")
endif (SDL2_FOUND)

find_package(PkgConfig QUIET)

#查找ffmpeg/libutil是否安装
if(PKG_CONFIG_FOUND)
    pkg_check_modules(AVUTIL QUIET IMPORTED_TARGET libavutil)
    if(AVUTIL_FOUND)
        include_directories(${AVUTIL_INCLUDE_DIRS})
        link_directories(${AVUTIL_LIBRARY_DIRS})
        list(APPEND  LINK_LIB_LIST ${AVUTIL_LIBRARIES})
        message(STATUS "found library:${AVUTIL_LIBRARIES}")
    endif()
else()
    find_package(AVUTIL QUIET)
    if(AVUTIL_FOUND)
        include_directories(${AVUTIL_INCLUDE_DIR})
        list(APPEND  LINK_LIB_LIST ${AVUTIL_LIBRARIES})
        message(STATUS "found library:${AVUTIL_LIBRARIES}")
    endif()
endif()

#查找ffmpeg/libavcodec是否安装
if(PKG_CONFIG_FOUND)
    pkg_check_modules(AVCODEC QUIET IMPORTED_TARGET libavcodec)
    if(AVCODEC_FOUND)
        include_directories(${AVCODEC_INCLUDE_DIRS})
        link_directories(${AVCODEC_LIBRARY_DIRS})
        list(APPEND  LINK_LIB_LIST ${AVCODEC_LIBRARIES})
        message(STATUS "found library:${AVCODEC_LIBRARIES}")
    endif()
else()
    find_package(AVCODEC QUIET)
    if(AVCODEC_FOUND)
        include_directories(${AVCODEC_INCLUDE_DIR})
        list(APPEND  LINK_LIB_LIST ${AVCODEC_LIBRARIES})
        message(STATUS "found library:${AVCODEC_LIBRARIES}")
    endif()
endif()

#查找ffmpeg/libavfilter 是否安装
if(PKG_CONFIG_FOUND)
	pkg_check_modules(AVFILTER QUIET IMPORTED_TARGET libavfilter)
	if(AVFILTER_FOUND)
		include_directories(${AVFILTER_INCLUDE_DIRS})
		link_directories(${AVFILTER_LIBRARY_DIRS})
		list(APPEND  LINK_LIB_LIST ${AVFILTER_LIBRARIES})
		message(STATUS "found library:${AVFILTER_LIBRARIES}")
    endif()
else()
	find_package(AVFILTER QUIET)
	if(AVFILTER_FOUND)
		include_directories(${AVFILTER_INCLUDE_DIR})
		list(APPEND  LINK_LIB_LIST ${AVFILTER_LIBRARIES})
		message(STATUS "found library:${AVFILTER_LIBRARIES}")
    endif()
endif()

#查找ffmpeg/libavformat 是否安装
if(PKG_CONFIG_FOUND)
	pkg_check_modules(AVFORMAT QUIET IMPORTED_TARGET libavformat)
	if(AVFORMAT_FOUND)
		include_directories(${AVFORMAT_INCLUDE_DIRS})
		link_directories(${AVFORMAT_LIBRARY_DIRS})
		list(APPEND  LINK_LIB_LIST ${AVFORMAT_LIBRARIES})
		message(STATUS "found library:${AVFORMAT_LIBRARIES}")
    endif()
else()
	find_package(AVFORMAT QUIET)
	if(AVFORMAT_FOUND)
		include_directories(${AVFORMAT_INCLUDE_DIR})
		list(APPEND  LINK_LIB_LIST ${AVFORMAT_LIBRARIES})
		message(STATUS "found library:${AVFORMAT_LIBRARIES}")
    endif()
endif()





aux_source_directory(. TEST_SRC_LIST)
#如果ffmpeg/libavcodec ffmpeg/libavcodec SDL 都安装了则编译 test_player
if(SDL2_FOUND AND AVCODEC_FOUND AND AVUTIL_FOUND)
else()
    message(STATUS "test_player ingored, please install sdl2 ffmpeg/libavcodec ffmpeg/libavutil")
    list(REMOVE_ITEM TEST_SRC_LIST ./test_player.cpp)
endif()

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

