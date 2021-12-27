/******************************************************************************
 *
 *       Filename:  audio_play.h
 *
 *    Description:  audio play
 *
 *        Version:  1.0
 *        Created:  2021年12月14日 17时34分17秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
/*!
* \file audio_play.h
* \brief 通过声卡播放生意
* 
* 
* \author yangkun
* \version 1.0
* \date 2021.12.14
*/
#ifndef _AUDIO_PLAY_H_
#define _AUDIO_PLAY_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "audio_recv.h"


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

/**
 * @brief 创建声音播放handle 以s16, 48k， 立体声方式打开
 * @param[in] 0 hdmi, 1 3.5
 * @return 0 success , other failed
 */
int audio_play_create(FHANDLE *hd,const int type);

/**
 * @brief 删除声音播放handle
 * @param[in] *hd 播放声音handle
 * @return 0 success, other failed
 */
int audio_play_remove(FHANDLE *hd);

/**
 * @brief 播放声音
 * @param[in] hd 播放声音handle
 * @param[in] buf 要播放的声音帧buf
 * @param[in] buf_size 帧大小
 * @return 0 success, other failed
 */
int audio_play_out(FHANDLE hd, void *buf, unsigned int buf_size);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif//_AUDIO_PLAY_H_

