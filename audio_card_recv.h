/******************************************************************************
 *
 *       Filename:  audio_card_recv.h
 *
 *    Description:  audio card recv
 *
 *        Version:  1.0
 *        Created:  2021年12月15日 14时05分49秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
/*!
* \file audio_card_recv.h
* \brief 接收 声卡数据
* 
* 
* \author yangkun
* \version 1.0
* \date 2021.12.15
*/
#ifndef _AUDIO_CARD_RECV_H_
#define _AUDIO_CARD_RECV_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <assert.h>


#include "sound_card_get.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */


/**
 * @brief 声卡声音数据获取回调
 * @buf 声音数据
 */
typedef void (*audio_card_recv_cb)(void *buf, unsigned int buf_size, long handle);

/**
 * @brief 创建声音接收handle
 * @param[in] name  声卡名字
 * @param[in] info 打开声卡信息
 * @param[in] _cb  音频数据回调
 * @param[in] handle 音频数据回调handle
 * @return 0 success, other failed
 */
int audio_card_recv_create(FHANDLE *hd, const char *name, sound_card_info *info,  audio_card_recv_cb _cb, long handle);


/**
 * @brief 产出音频接受handle
 * @return 0 success, other failed
 */
int audio_card_recv_remove(FHANDLE *hd);




#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif//_AUDIO_CARD_RECV_H_
