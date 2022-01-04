/******************************************************************************
 *
 *       Filename:  test_wav_play.c
 *
 *    Description:  wav read and play
 *
 *        Version:  1.0
 *        Created:  2022年01月04日 14时55分46秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
/*!
* \file test_wav_play.c
* \brief 读取wav文件，s16, 48k， 立体声pcm数据 从声卡播放出去 
* 
* 
* \author yangkun
* \version 1.0
* \date 2022.1.4
*/
#include "audio_recv.h"
#include "audio_play.h"
#include "audio_card_recv.h"
#include "wavreader.h"

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>



int is_start = 0;
int usb_is_start = 0;
void set_mix_exit() {
	is_start = 0;
	usb_is_start = 0;
}
static void sigterm_handler(int sig) {
	printf("get sig:%d\n", sig);
	set_mix_exit();
}

FHANDLE play_hdmi_hd = NULL;
int test_wav_read()
{
	FILE *out;
	void *wav;
	int format, sample_rate, channels, bits_per_sample;
	unsigned int input_size;
	uint8_t* input_buf;
	const char *infile = "founders.wav";

	int read, i;

	wav = wav_read_open(infile);
	if (!wav) {
		fprintf(stderr, "Unable to open wav file %s\n", infile);
		return 1;
	}
	if (!wav_get_header(wav, &format, &channels, &sample_rate, &bits_per_sample, NULL)) {
		fprintf(stderr, "Bad wav file %s\n", infile);
		return 1;
	}
	printf("format:%d, channels:%d sample_rate:%d bits_per_sample:%d\n",
			format, channels, sample_rate, bits_per_sample);
	if (format != 1) {
		fprintf(stderr, "Unsupported WAV format %d\n", format);
		return 1;
	}
	if (bits_per_sample != 16 && sample_rate!= 48000 && channels !=2) {
		fprintf(stderr, "Unsupported WAV sample depth %d\n", bits_per_sample);
		return 1;
	}

	input_size = channels*2*1024;
	input_buf = (uint8_t*) malloc(input_size);

	int count = 0;
	void *data[1];
	int size = 1024;
	// 循环读取wav文件
	while (is_start) 
	{
		printf("input_size:%d\n", input_size);
		read = wav_read_data(wav, input_buf, input_size);
		if(read <= 0)
		{
			break;
		}

		data[0] = input_buf;
		// 将数据发送到编码器
		audio_play_out(play_hdmi_hd, data, size);

		// 进行延时 发送过快，会自动阻塞
		//usleep(20*1000);

		printf("count=%d, read=%d \n", count, read);
		count ++;
	}


	wav_read_close(wav);
	free(input_buf);
	input_buf = NULL;
	return 0;
}

int main()
{
	int ret;

	//void *data[1];
	//data[0] = (void*)audio_data;

	signal(SIGINT, sigterm_handler);
	is_start = 1;

	ffmpeg_init();

	int hdmi_type = 0;

	printf("==========================>%d \n", __LINE__);
	// 打开声卡
	ret = audio_play_create(&play_hdmi_hd, hdmi_type);
	printf("==========================>%d %p\n", __LINE__, play_hdmi_hd);
	assert(ret == 0);


	test_wav_read();

	ret = audio_play_remove(&play_hdmi_hd);
	assert(ret == 0);

}
