/******************************************************************************
 *
 *       Filename:  audio_recv.h
 *
 *    Description:  音频接收
 *
 *        Version:  1.0
 *        Created:  2021年12月14日 15时50分20秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
/*!
* \file audio_recv.h
* \brief 接收rtsp音频文件
* 
* 
* \author yangkun
* \version 1.0
* \date 2021.12.14
*/
#ifndef _AUDIO_RECV_H_
#define _AUDIO_RECV_H_

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

typedef  void* FHANDLE;

/**
 * @brief 初始化ffmpeg 日志注册信息等
 * @return 0 success other failed
 */
int ffmpeg_init();

/**
 * @brief 声音数据获取回调
 * @buf 声音数据
 */
typedef void (*audio_recv_cb)(void *buf, unsigned int buf_size, long handle);

/**
 * @brief 创建声音接收handle
 * @param[in] url   播放url
 * @param[in] _cb  音频数据回调
 * @param[in] handle 音频数据回调handle
 * @return 0 success, other failed
 */
int audio_recv_create(FHANDLE *hd, const char *url, audio_recv_cb _cb, long handle);


/**
 * @brief 产出音频接受handle
 * @return 0 success, other failed
 */
int audio_recv_remove(FHANDLE *hd);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
#endif//_AUDIO_RECV_H_
