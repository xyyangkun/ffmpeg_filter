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
#include "audio_play.h"
#include "audio_card_recv.h"

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

// 从rtsp接收数据解码后通过声卡播放
int test_rtsp_recv_card_play()
{
	FHANDLE hd = NULL;
	FHANDLE play_hdmi_hd = NULL;
	FHANDLE recv_hdmi_hd = NULL;
	FHANDLE recv_35_hd = NULL;

	struct s_play_hd play_hd;
	int ret;
	int hdmi_type = 0;
	int _35_type = 1;
	char *rtsp_url1 = "rtsp://172.20.2.7/h264/720p";

	char *rtsp_url2 = "rtsp://172.20.2.7/h264/1080p";

	ffmpeg_init();

	signal(SIGINT, sigterm_handler);

	is_start = 1;


	//ret = audio_play_create(&play_hdmi_hd, audio_hdmi_card_name);
	ret = audio_play_create(&play_hd.play_hdmi_hd, hdmi_type);
	ret = audio_play_create(&play_hd.play_35_hd, _35_type);
	printf("==========================>%d %p\n", __LINE__, play_hdmi_hd);
	assert(ret == 0);

	//ret = audio_recv_create(&hd, rtsp_url, audio_recv,  play_hdmi_hd); 
	ret = audio_recv_create(&recv_hdmi_hd, rtsp_url1, audio_recv,  (long)play_hd.play_hdmi_hd); 
	assert(ret == 0);
	ret = audio_recv_create(&recv_35_hd, rtsp_url2, audio_recv,  (long)play_hd.play_35_hd); 
	assert(ret == 0);



	while(is_start == 1)
	{
		usleep(100*1000);

	}
	printf("==============> %s %d\n", __FUNCTION__, __LINE__);

	//ret = audio_recv_remove(&hd);
	ret = audio_recv_remove(&recv_hdmi_hd);
	assert(ret == 0);
	ret = audio_recv_remove(&recv_35_hd);
	assert(ret == 0);

	printf("==============> %s %d\n", __FUNCTION__, __LINE__);

	//ret = audio_play_remove(&play_hdmi_hd);
	ret = audio_play_remove(&play_hd.play_hdmi_hd);
	assert(ret == 0);
	ret = audio_play_remove(&play_hd.play_35_hd);
	assert(ret == 0);
	printf("==============> %s %d\n", __FUNCTION__, __LINE__);
	assert(hd == NULL);
	return 0;
}


void audio_card_recv(void *buf, unsigned int size, long handle)
{
	FHANDLE hd = (FHANDLE)handle;
	printf("audio_card_recv size:%d\n", size);
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
	sound_card_info info;
	char dev_name[100]= {0};
	if(0 != found_sound_card(dev_name))
	{
		printf("not found sound card\n");
		return -1;
	}

	if(0 != get_sound_card_info(dev_name, &info))
	{
		printf("not support sound card or sound card not exist\n");
		return -1;
	}


	ret = audio_card_recv_create(&hd, dev_name, &info, audio_card_recv, (long)play_hdmi_hd);
	assert(ret==0);

	while(is_start == 1)
	{
		usleep(100*1000);

	}
	printf("==============> %s %d\n", __FUNCTION__, __LINE__);

	
	ret = audio_card_recv_remove(&hd);
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

// 从声卡名字获取声卡序号
int  test_card_name()
{
	char *names[] = {"rockchip_hdmi", "rockchip_rk809", "rockchipdummyco", "rockchipdummy_1"};
	printf("sizeof names:%d\n", sizeof(names));
	for(int i=0; i<sizeof(names)/sizeof(char *); i++)
	{
		char dev_name[100]= {0};
		if(0 != found_sound_card1(names[i], dev_name))
		{
			printf("not found sound card\n");
			return -1;
		}
		printf("name %d=%s\n", i, dev_name);
	}


	return 0;
}

int main()
{
	//test_rtsp_recv_card_play();
	//test_card_recv_card_play();
	//test_mute_card_play();
	test_card_name();
}
