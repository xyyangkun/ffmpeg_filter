/******************************************************************************
 *
 *       Filename:  audio_card_get.c
 *       编译运行方法：
 *       gcc linux_pcm_save.c -lasound
 *       ./a.out hw:0 123.pcm
 *
 *    Description:  获取声卡属性
 *    采样位数8 16
 *    采样格式：
 *    声道数: 
 *    采样频率：8k 16k 32k 48k... 
 *    https://www.cnblogs.com/L-102/p/11488030.html
 *    https://www.freesion.com/article/5211378269/
 *    https://www.codenong.com/cs105368195/
 *
 *    ioctl函数中相关的命令如下：
 *    SOUND_PCM_WRITE_BITS：设置声卡的量化位数，8或者16，有些声卡不支持16位；
 *    SOUND_PCM_READ_BITS：获取当前声卡的量化位数；
 *    SOUND_PCM_WRITE_CHANNELS：设置声卡的声道数目，1或者2，1为单声道，2为立体声；
 *    SOUND_PCM_READ_CHANNELS：获取当前声卡的声道数；
 *    SOUND_PCM_WRITE_RATE：设置声卡的采样频率，8K，16K等等；
 *    SOUND_PCM_READ_RATE：获取声卡的采样频率
 *    
 *    程序流程
 *	  1. 从/proc/asounds/cards 下面获取usb声卡名字
 *	  2. 打开声卡名字，获取属性列表
 *	  3. 选择最佳支持列表
 *
 *	 
 *
 *        Version:  1.0
 *        Created:  2021年11月26日 09时38分06秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <signal.h>
#include "sound_card_get.h"
 
// 各级优先级配置打开声卡
static sound_card_info _info[4] = {
{
	128,
	0, // S16_LE 其他不支持
	2, // 双声道
	48000,// 48k采样率
},
{
	128,
	0, // S16_LE 其他不支持
	2, // 双声道
	16000,// 16k采样率
},
{
	128,
	0, // S16_LE 其他不支持
	1, // 单声道
	48000,// 48k采样率
},
{
	128,
	0, // S16_LE 其他不支持
	1, // 单声道
	16000,// 16k采样率
},
};


// 尝试使用已知配置打开声卡,返回成功则认为此声卡可用
static int _try_card_open(const char *name, snd_pcm_format_t format, unsigned int sample, unsigned int channels)
{
	int err;

	snd_pcm_t *capture_handle=NULL;// 一个指向PCM设备的句柄
	snd_pcm_hw_params_t *hw_params = NULL; //此结构包含有关硬件的信息，可用于指定PCM流的配置
	err = 0;
 
	/*打开音频采集卡硬件，并判断硬件是否打开成功，若打开失败则打印出错误提示*/
	if ((err = snd_pcm_open (&capture_handle, name,SND_PCM_STREAM_CAPTURE,0))<0) 
	{
		printf("无法打开音频设备: %s (%s)\n", name, snd_strerror (err));
		err = -1;
		return -1;
	}
	printf("音频接口打开成功.\n");
 
	/*分配硬件参数结构对象，并判断是否分配成功*/
	if((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) 
	{
		printf("无法分配硬件参数结构 (%s)\n",snd_strerror(err));
		err = -2;
		return -2;
	}
	printf("硬件参数结构已分配成功.\n");
	
	/*按照默认设置对硬件对象进行设置，并判断是否设置成功*/
	if((err=snd_pcm_hw_params_any(capture_handle,hw_params)) < 0) 
	{
		printf("无法初始化硬件参数结构 (%s)\n", snd_strerror(err));
		err = -3;
		goto end;
	}
	printf("硬件参数结构初始化成功.\n");
 
	/*
		设置数据为交叉模式，并判断是否设置成功
		interleaved/non interleaved:交叉/非交叉模式。
		表示在多声道数据传输的过程中是采样交叉的模式还是非交叉的模式。
		对多声道数据，如果采样交叉模式，使用一块buffer即可，其中各声道的数据交叉传输；
		如果使用非交叉模式，需要为各声道分别分配一个buffer，各声道数据分别传输。
	*/
	if((err = snd_pcm_hw_params_set_access (capture_handle,hw_params,SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) 
	{
		printf("无法设置访问类型(%s)\n",snd_strerror(err));
		err = -4;
		goto end;
	}
	printf("访问类型设置成功.\n");
 
	/*设置数据编码格式，并判断是否设置成功*/
	if ((err=snd_pcm_hw_params_set_format(capture_handle, hw_params,format)) < 0) 
	{
		printf("无法设置格式 (%s)\n",snd_strerror(err));
		err = -5;
		goto end;
	}
	fprintf(stdout, "PCM数据格式设置成功.\n");
 
	/*设置采样频率，并判断是否设置成功*/
	if((err=snd_pcm_hw_params_set_rate_near (capture_handle,hw_params,&sample,0))<0) 
	{
		printf("无法设置采样率(%s)\n",snd_strerror(err));
		err = -6;
		goto end;
	}
	printf("采样率设置成功\n");
 
	/*设置声道，并判断是否设置成功*/
	if((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, channels)) < 0) 
	{
		printf("无法设置声道数(%s)\n",snd_strerror(err));
		err = -7;
		goto end;
	}
	printf("声道数设置成功.\n");
 
	/*将配置写入驱动程序中，并判断是否配置成功*/
	if ((err=snd_pcm_hw_params (capture_handle,hw_params))<0) 
	{
		printf("无法向驱动程序设置参数(%s)\n",snd_strerror(err));
		err = -8;
		goto end;
	}
	printf("参数设置成功.\n");

end:
	assert(hw_params);
	assert(capture_handle);
	// 释放内存
	if(hw_params)
		snd_pcm_hw_params_free(hw_params);

	/*关闭音频采集卡硬件*/
	if(capture_handle)
		snd_pcm_close(capture_handle);
	return err;
}

//通过声卡名字获取声卡采样率，声道数，数据格式信息
int get_sound_card_info(const char *name, sound_card_info *info) {

	int buffer_frames = 128;
	unsigned int sample;
	unsigned int channels;
	/*PCM的采样格式在pcm.h文件里有定义*/
	snd_pcm_format_t format=SND_PCM_FORMAT_S16_LE; // 采样位数：16bit、LE格式


	int size = sizeof(_info)/sizeof(sound_card_info);
	int i =0;
	int ret;
	for(i=0; i<size; i++)
	{
		sample = _info[i].sample;
		channels = _info[i].channels;
		if(0 == (ret =  _try_card_open(name, format, sample, channels)))
		{
			// 将验证可以打开的配置返回回去
			memcpy(info, &_info[i], sizeof(sound_card_info));
			break;
		}
		// 打开声卡失败，直接退出
		if(ret == -1 || ret == -2 || ret == -3) {
			break;
		}
	}
	if(ret == 0) {
		printf("success get support property form %s, sample:%d channels:%d\n",
				name, sample, channels);
	}
	return ret;
}
 
