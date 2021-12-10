/******************************************************************************
 *
 *       Filename:  test_demux.c
 *
 *    Description:  test demux
 *
 *        Version:  1.0
 *        Created:  2021年12月10日 11时31分14秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
/*!
* \file test_demux.c
* \brief 解析mp4文件
* 
* 
* \author yangkun
* \version 1.0
* \date 2021.12.10
*/

#include "demux.h"
FILE *fp_video = NULL;
FILE *fp_audio = NULL;

struct s_param{
	FILE *fp_video;
	FILE *fp_audio;
	int nouse;
};

struct s_param g_param;

int fp_init() {
	assert(!fp_video);
	assert(!fp_audio);
	fp_video = fopen("output.h264", "wb");
	if(!fp_video) {
		printf("ERROR to open vide file:%s\n", strerror(errno));
		exit(1);
	}
	fp_audio = fopen("output.pcm", "wb");
	if(!fp_video) {
		printf("ERROR to open audio file:%s\n", strerror(errno));
		exit(1);
	}
	g_param.fp_video = fp_video;
	g_param.fp_audio = fp_audio;
	return 0;
}
int fp_deinit() {
	fclose(fp_video);	
	fclose(fp_audio);	
	fp_video = NULL;
	fp_audio = NULL;
	memset(&g_param, 0 ,sizeof(g_param));
	return 0;
}


// 解析mp4数据帧回调
void file_write(void *buf, unsigned int buf_size, unsigned int type, long handle)
{
	char head[4]= {0x0, 0, 0, 1};
	struct s_param *param = (struct s_param *)handle;
	assert(handle);
	if(type == 1 && param->fp_audio!= NULL)
	{
		fwrite(buf, 1, buf_size, param->fp_audio);
	}

	// h264
	if(type == 2 && param->fp_video!= NULL)
	{
		//fwrite(head, 1, 4, param->fp_video);
		fwrite(buf, 1, buf_size, param->fp_video);
	}
}

int is_start = 0;
// MP4读取结束回调
void file_close(unsigned int type, long handle) {
	struct s_param *param = (struct s_param *)handle;
	printf("mp4 file close , type=%d\n", type);
	is_start = 0;
}


void _signal(int signo)
{
	signal(SIGINT, SIG_IGN); 
	signal(SIGTERM, SIG_IGN);

	printf("get signal:%d\n", signo);

	is_start = 0;

}

//char *path = "/nfsroot/rk3568/rk3568_1080p_h264.mp4";
//char *path = "/nfsroot/rk3568/blackmagicdesign_p30.mp4";
char *path = "/nfsroot/rk3568/h264_720p_founders.mp4";

int main() {
	
	int ret;
	ret = fp_init();
	assert(ret == 0);
	printf("1>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	ret = open_local_file(path, file_write, file_close, (long)&g_param);

	printf("2>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

	signal(SIGINT, _signal);
	signal(SIGTERM, _signal);


	is_start = 1;
	while(is_start == 1) {
		sleep(1);
	}
	printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

	close_local_file();
	return 0;
}
