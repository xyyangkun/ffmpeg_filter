/******************************************************************************
 *
 *       Filename:  demux.h
 *
 *    Description:  解析mp4文件
 *
 *        Version:  1.0
 *        Created:  2021年12月10日 10时11分07秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
/*!
* \file demux.h
* \brief 解析mp4文件
* 
* 
* \author yangkun
* \version 1.0
* \date 2021.12.10
*/
#ifndef _DEMUX_H_
#define _DEMUX_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <assert.h>



#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

/**
 * @brief 解析本地mp4a文件时,得到的音频视频帧文件回调
 * @param[in] buf 音视频内容
 * @param[in] buf_size buf长度
 * @param[in] type 1 音频, 2 h264视频, 3 h265视频 
 * @param[in] handle 回调函数需要的句柄
 */
typedef void (*local_av_callback)(void* buf, unsigned int buf_size, 
		unsigned int type,
		long handle);

/**
 * @brief 解析本地mp4a文件时,得到的音频视频帧文件回调
 * @param[in] buf_handle 音视频内容 handle,   音频解码后的AVFrame,  视频packet
 * @param[in] handle 回调函数需要的句柄
 */
typedef void (*local_av_callback_v1)(void* buf_handle, 
		long handle);



/**
 * @brief 文件读取结束或者主动调用文件关闭回调
 * @param[in] type 1 文件读完， 2 文件未读完，上层关闭读取操作
 * @param[in] handle 回调函数需要的句柄
 */
typedef void (*local_av_close_callback)( 
		unsigned int type,
		long handle);




/**
 * @brief 文件读取结束回调
 *
 */

/**
 * @brief 打开本地文件
 * @param[in] filename 带路径的文件名
 * @param[in] cb 回调函数
 * @param[in] handle 回调函数使用的handle
 * @return 0 success, other failed
 */
int open_local_file(char *filename, local_av_callback _cb, local_av_close_callback _close_cb, long handle);


/**
 * @brief 设置mp4音视频数据回调
 * @param[in] _audio_cb 声音回调函数
 * @param[in] _video_cb 视频回调函数
 * @param[in] handle  回调函数handle
 * @return 0 success, other failed
 */
int set_local_file_cb_v1(local_av_callback_v1 _audio_cb, local_av_callback_v1 _video_cb, long handle);

/**
 * @brief 关闭本地文件
 * @return 0 success,other failed
 */
int close_local_file();


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
#endif//_DEMUX_H_
