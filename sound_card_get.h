/******************************************************************************
 *
 *       Filename:  audio_card_get.h
 *
 *    Description:  获取声卡信息
 *
 *        Version:  1.0
 *        Created:  2021年11月26日 10时24分49秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
#ifndef _AUDIO_CARD_H_
#define _AUDIO_CARD_H_

/**
 * @struct s_sound_card_info
 * @brief 声卡信息结构
 */
typedef struct s_sound_card_info
{
	int buffer_frames; ///< default = 128
	int format ;       ///< 数据格式 0 S16_LE 其他不支持
	int channels ;     ///< 声道数   1 单声道 2 双声道， 其他不支持
	int sample;        ///< 采样率 16000 16k    32000 32k  48000 48k  其他不支持
}sound_card_info;

/**
 * @brief 通过声卡名字,打开声卡，确认是否支持采样率声道数据等信息
 * @param[in] name 声卡命令
 * @param[in] info 声卡信息
 * @return 0:success, other:failure
 */
int get_sound_card_info(const char *name, sound_card_info *info);

#endif//_AUDIO_CARD_H_
