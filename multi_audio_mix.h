/******************************************************************************
 *
 *       Filename:  multi_audio_mix.h
 *
 *    Description:  i
 *
 *        Version:  1.0
 *        Created:  2021年11月30日 09时39分42秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
#ifndef _MULTI_AUDIO_MIX_H_
#define _MULTI_AUDIO_MIX_H_

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

void* multi_audio_mix_proc(void *arg);
int set_mix_volume(unsigned char index, const char *volume);
void set_mix_exit();

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
#endif//_MULTI_AUDIO_MIX_H_
