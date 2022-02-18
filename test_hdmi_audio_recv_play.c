/******************************************************************************
 *
 *       Filename:  test_recv_play.c
 *
 *    Description:  test audio recv play
 *
 *        Version:  1.0
 *        Created:  2021年12月14日 19时28分38秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
#include "audio_recv.h"
#include "hdmi_audio_card_recv.h"
#include "audio_play.h"

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

struct s_play_hd
{
	FHANDLE play_hdmi_hd;
	FHANDLE play_35_hd; 
};



void audio_recv(void *buf, unsigned int size, long handle)
{
	//printf("recv audio buf size:%d, handle:%lld\n", size, handle);
	assert(handle!=0);
#if 1
	FHANDLE hd = (FHANDLE)handle;
	audio_play_out(hd, buf, size);
#else
	struct s_play_hd* p_play_hd = (struct s_play_hd *)handle;

	//audio_play_out(hd, buf, size);
	if(p_play_hd->play_hdmi_hd)
	audio_play_out(p_play_hd->play_hdmi_hd, buf, size);
	if(p_play_hd->play_35_hd)
	audio_play_out(p_play_hd->play_35_hd, buf, size);
#endif
}


void hdmi_audio_card_recv(void *buf, unsigned int size, long handle)
{
	FHANDLE hd = (FHANDLE)handle;
	printf("hdmi_audio_card_recv size:%d\n", size);
	audio_play_out(hd, buf, size);

}

//  从声卡读取数据，从声卡播放
int  test_card_recv_card_play()
{
	FHANDLE hd = NULL;
	int ret;
	int hdmi_type = 0;

	ffmpeg_init();

	signal(SIGINT, sigterm_handler);

	is_start = 1;

	// 打开声卡
	FHANDLE play_hdmi_hd = NULL;
	ret = audio_play_create(&play_hdmi_hd, hdmi_type);
	printf("==========================>%d %p\n", __LINE__, play_hdmi_hd);
	assert(ret == 0);


	// 动态获取摄像头声卡参数
	sound_card_info info = {
		1024,   // buffer_frames
		0,      // s16_le
		2,      // 立体声
		48000,  // 48k 采样率
	};
	char dev_name[100]= "hw:2";
	//char dev_name[100]= "hw:3";

	ret = hdmi_audio_card_recv_create(&hd, dev_name, &info, hdmi_audio_card_recv, (long)play_hdmi_hd);
	assert(ret==0);

	while(is_start == 1)
	{
		usleep(100*1000);

	}
	printf("==============> %s %d\n", __FUNCTION__, __LINE__);

	
	ret = hdmi_audio_card_recv_remove(&hd);
	assert(ret==0);

	ret = audio_play_remove(&play_hdmi_hd);
	assert(ret == 0);

	printf("==============> %s %d\n", __FUNCTION__, __LINE__);
}

static char audio_data[1024*4]={0};
// 测试播放静音数据
int  test_mute_card_play()
{
	int ret;

	int size = 1024;
	void *data[1];
	data[0] = (void*)audio_data;

	signal(SIGINT, sigterm_handler);
	is_start = 1;

	ffmpeg_init();

	int hdmi_type = 0;

	printf("==========================>%d \n", __LINE__);
	// 打开声卡
	FHANDLE play_hdmi_hd = NULL;
	ret = audio_play_create(&play_hdmi_hd, hdmi_type);
	printf("==========================>%d %p\n", __LINE__, play_hdmi_hd);
	assert(ret == 0);


	while(is_start == 1) {
		audio_play_out(play_hdmi_hd, data, size);
		usleep(20*1000);
	}

	ret = audio_play_remove(&play_hdmi_hd);
	assert(ret == 0);
}

int main()
{
	test_card_recv_card_play();
	//test_mute_card_play();
}
