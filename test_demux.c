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
#include <libavutil/log.h>
 #include <libavformat/avformat.h>
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

// 获取mp4视频支持格式
// http://kevinnan.org.cn/index.php/archives/571/
int test_getinfo(char *path)
{
	int result;
	AVFormatContext *fmt_ctx = NULL;
	av_log_set_level(AV_LOG_INFO);
	av_register_all();

	result = avformat_open_input(&fmt_ctx, path, NULL, NULL);
	if(result < 0){
		av_log(NULL,AV_LOG_ERROR,"can't open file : %s \n",av_err2str(result));
		return -1;
	}

	/* retrieve stream information */
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		fprintf(stderr, "Could not find stream information\n");
		return -2;
	}

    //计算视频长度
    int hours, mins, secs;
    secs = fmt_ctx->duration / 1000000;
    mins = secs / 60;
    secs %= 60;
    hours = mins / 60;
    mins %= 60;

    //格式化视频长度
    char duration_foramt_[128];
    sprintf(duration_foramt_, "%d:%d:%d", hours, mins, secs);
	printf("duration :%s\n", duration_foramt_);

	// 流数量
	printf("stream num:%d\n", fmt_ctx->nb_streams);

	av_dump_format(fmt_ctx, 0, "./test.mp4", 0);

	printf("h264 type:%d h265 type:%d aac type:%d \n", 
			AV_CODEC_ID_H264, 
			AV_CODEC_ID_HEVC,
			AV_CODEC_ID_AAC);

	// 遍历流
	for(int i =0; i<fmt_ctx->nb_streams; i++)
	{
		//取出一路流,并生成AVStream对象
		AVStream* input_stream = fmt_ctx->streams[i];
		//判断是否为视频流
		if(input_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){

			//avg_frame_rate -> AVRational(有理数),
			//avg_frame_rate.num : 分子
			//avg_frame_rate.den : 母
			//得到视频帧率
			int frame_rate_ = input_stream->avg_frame_rate.num / input_stream->avg_frame_rate.den;
			
			//取出视频流中的编码参数部分, 生成AVCodecParamters对象
            AVCodecParameters* codec_par = input_stream->codecpar;

            //利用编码参数对象AVCdecParamters得到视频宽度，高度，码率，视频大小
            int width_ = codec_par->width;
            int height_ = codec_par->height;
            int video_average_bit_rate_ = codec_par->bit_rate/1000;
            //int video_size_ = this->video_average_bit_rate_ * secs / (8.0*1024);
			int code_id = codec_par->codec_id;
			
			printf("video w:%d h:%d rate:%d code_id:%d\n", 
				width_, height_, video_average_bit_rate_, code_id);
			printf("code name:%s\n", avcodec_get_name(codec_par->codec_id));
		}
		else if(input_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			//生成AVcodecParamters对象
            AVCodecParameters* codec_par = input_stream->codecpar;
			
			int audio_average_bit_rate_ = codec_par->bit_rate / 1000;
            int channel_nums = codec_par->channels;
            int sample_rate_ = codec_par->sample_rate;
			printf("audio bit rate:%d, channel_nums:%d sample_rate:%d, code_id:%s\n",
				audio_average_bit_rate_, channel_nums, sample_rate_,
				avcodec_get_name(codec_par->codec_id));	
		}

	}


	avformat_close_input(&fmt_ctx);



	return 0;
}




//char *path = "/nfsroot/rk3568/rk3568_1080p_h264.mp4";
//char *path = "/nfsroot/rk3568/blackmagicdesign_p30.mp4";
char *path = "/nfsroot/rk3568/h264_720p_founders.mp4";
char *path_265 = "/nfsroot/rk3568/rk3568_1080p_h265.mp4";

int main() {

	return test_getinfo(path_265);
	
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
