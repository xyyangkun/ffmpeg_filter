/******************************************************************************
 *
 *       Filename:  rtsp_recv.c
 *
 *    Description:  ffmpeg rtsp recv and save
 *		// 接收rtsp 音视频数据并保存
 *
 *        Version:  1.0
 *        Created:  2021年11月19日 14时54分07秒
 *       Revision:  none
 *       Compiler:  gcc
 *   https://www.codenong.com/cs106186144/
 *   保存aac类似h264有个7字节的aac头
 *   https://blog.csdn.net/andrewgithub/article/details/109589400
 *   https://blog.csdn.net/esdhhh/article/details/95871036
 *   https://www.jianshu.com/p/c8488537501b
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/

#include "libavcodec/avcodec.h"
#include <libavformat/avformat.h>
#include "libavformat/avio.h"

#include "libavutil/channel_layout.h"
#include "libavutil/md5.h"
#include "libavutil/opt.h"
#include "libavutil/samplefmt.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/frame.h"
#include "libavutil/opt.h"

#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include <libavfilter/avfiltergraph.h>

#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

// rtsp 回调，连接rtsp成功后，接收server端发来的音视频数据
// type = 0 视频 1 音频
typedef void (*rtsp_cb)(void *buf, int len, int time, int type, long param);

int is_start = 0;
static void sigterm_handler(int sig) {
	printf("get sig:%d\n", sig);
	is_start = 0;
}



static AVFormatContext *ifmt_ctx = NULL;
rtsp_cb cb;
static int videoindex;
static int audioindex;
pthread_t recv_id;

static struct timeval start_time;
static struct timeval end_time;

struct s_out_file
{
	FILE* video;
	FILE* audio;
};



struct s_out_file out_file;

void _rtsp_cb(void *buf, int len, int time, int type, long param)
{
	//printf("len = %d\n", len);
	if(param != 0) {
		struct s_out_file *p_out = (struct s_out_file *)param;
		if(type == 0) {
			fwrite(buf, len, 1, p_out->video);
		} else {
			fwrite(buf, 1, len, p_out->audio);
		}
	}
}



void *recv_proc(void *param)
{
	int ret;
	AVPacket pkt;
	int64_t start_time=0;
	int64_t pts=0;
	int64_t dts=0;
	int frame_index=0;

	int freqIdx = 0x3;//48000
	int channel = 0;
	int profile = 0;
	
	int sample = 0;
	for (int i = 0; i < ifmt_ctx->nb_streams; ++i)
	{
		if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audioindex = i;
			sample = ifmt_ctx->streams[i]->codecpar->sample_rate;
			channel = ifmt_ctx->streams[i]->codecpar->channels;
			profile = ifmt_ctx->streams[i]->codecpar->profile;
			break;
		}
	}

	printf("sample=%d channel=%d profile=%d\n", sample, channel, profile);

	// 构造aac 头
	uint8_t padts[10];//
	memset(padts, 0, 10);
	padts[0] = (uint8_t)0xFF;
	padts[1] = (uint8_t)0xF1;
	padts[2] = (uint8_t)((profile << 6) + (freqIdx << 2) + (channel >> 2));
	padts[6] = (uint8_t)0xFC;

	while(is_start) {
		ret = av_read_frame(ifmt_ctx, &pkt);
		if(ret < 0) {
			printf("error to read frame\n");
			break;
		}

		if(pkt.pts == AV_NOPTS_VALUE) {
			// write pts
			AVRational time_base1=ifmt_ctx->streams[videoindex]->time_base;
            //Duration between 2 frames (us)
            int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(ifmt_ctx->streams[videoindex]->r_frame_rate);
            //计算 pts的公式
            pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
            pkt.dts=pkt.pts;
            pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
		}
		//Important:Delay
        if(pkt.stream_index==videoindex){
            AVRational time_base=ifmt_ctx->streams[videoindex]->time_base;
            AVRational time_base_q={1, AV_TIME_BASE};
            int64_t pts_time = av_rescale_q(pkt.dts, time_base, time_base_q);
            int64_t now_time = av_gettime() - start_time;
            if (pts_time > now_time)
                av_usleep(pts_time - now_time);

			frame_index++;
        }
		//printf("recv frame index:%d\n", frame_index);
		//printf("len:%d index:%d\n", pkt.size, pkt.stream_index);
#if 0
		unsigned char *p = (unsigned char *)pkt.data;
		printf("%02x %02x %02x %02x ", p[0], p[1], p[2], p[3]);
		printf("%02x %02x %02x %02x\n", p[4], p[5], p[6], p[7]);
#endif
	
		if(cb){
			// aac添加头
			if(pkt.stream_index == audioindex) {
				padts[3] = (uint8_t)(((channel & 3) << 6) + ((7 + pkt.size) >> 11));// 增加ADTS头
				padts[4] = (uint8_t)(((7 + pkt.size) >> 3) & 0xFF);
				padts[5] = (uint8_t)((((7 + pkt.size) & 7) << 5) + 0x1F);
				cb(padts, 7, (int)pkt.pts, pkt.stream_index, (long )param);
			}
			cb(pkt.data, pkt.size, (int)pkt.pts, pkt.stream_index, (long )param);
		}

		//释放内存
		av_free_packet(&pkt);
		//av_packet_unref(&pkt);
	}

}

static int _out = 0;
#define TIME_OUT 3000 // ms
static int interruptcb(void *ctx) {
	if(_out) return 0;
	
	gettimeofday(&end_time, NULL);
	float time_use = ((end_time.tv_sec-start_time.tv_sec)*1000000 +
			(end_time.tv_usec - start_time.tv_usec))/1000;
	if(time_use > TIME_OUT) {
		printf("check time out!!!!\n");
		return 1;
	}
	return 0;
}

int init_rtsp()
{
	int ret;
	size_t buffer_size, avio_ctx_buffer_size = 4096;
	//const char *in_filename = "rtsp://172.20.2.18/live/main";
	//const char *in_filename = "rtsp://172.20.2.18/h264/main";
	const char *in_filename = "rtsp://172.20.2.7/h264/720p";
	cb = _rtsp_cb;

	// 打开保存文件
	FILE* fp = fopen("video_out.h264", "wb");
	if(!fp) {
		printf("ERROR to open video out file\n");
		return -1;
	}
	out_file.video = fp;
	fp = fopen("audio_out.aac", "wb");
	if(!fp) {
		printf("ERROR to open audio out file\n");
		return -1;
	}
	out_file.audio = fp;

	// 设置超时等属性
	AVDictionary* options = NULL;
#if 1
	av_dict_set(&options, "rtsp_transport", "tcp", 0);
	av_dict_set(&options, "stimeout", "1000000", 0); //没有用

	ifmt_ctx = avformat_alloc_context();

	// 设置超时回调函数后上面的超时属性才生效, 可以在超时函数中处理其他逻辑
	AVIOInterruptCB icb = {interruptcb, NULL};
	ifmt_ctx->interrupt_callback = icb;
#endif


	// 记录当前时间
	gettimeofday(&start_time, NULL);

	// 初始化输入
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, &options)) < 0) {
		printf( "Could not open input file.");
		av_dict_free(&options);
		goto end;
	}
    av_dict_free(&options);
	// 连接成功后中断中不检查了
	_out = 1;

	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		printf( "Failed to retrieve input stream information");
		goto end;
	}
	for(int i=0; i<ifmt_ctx->nb_streams; i++)
		if(ifmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
			videoindex=i;
			break;
		}

	for(int i=0; i<ifmt_ctx->nb_streams; i++)
		if(ifmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
			audioindex=i;
			break;
		}

	printf("will demp format:\n");
	// 打印流媒体信息
	av_dump_format(ifmt_ctx, 0, in_filename, 0);
	
	is_start = 1;

	// 创建线程接收数据
	recv_id = 0;
	ret = pthread_create(&recv_id, NULL, recv_proc, (void *)&out_file);
	if(ret != 0){
		printf("error to create thread:errno = %d\n", errno);
		goto end;
	}

	return 0;

end:
	avformat_free_context(ifmt_ctx);
	avformat_close_input(&ifmt_ctx);
	ifmt_ctx = NULL;


	return -1;

}

int deinit_rtsp()
{
	is_start = 0;
	if(recv_id!=0) {
		recv_id = 0;
		pthread_join(recv_id, NULL);
	}

	if(ifmt_ctx) {
		avformat_close_input(&ifmt_ctx);
		//avformat_free_context(ifmt_ctx);
		ifmt_ctx = NULL;
	}

	if(out_file.video){
		fclose(out_file.video);
		out_file.video = NULL;
	}
	if(out_file.audio) {
		fclose(out_file.audio);
		out_file.audio = NULL;
	}

	return 0;
}

int main(int argc, const char * argv[])
{
	signal(SIGINT, sigterm_handler);

    av_log_set_level(AV_LOG_VERBOSE);
    

    int err;
	
	av_register_all();
	avfilter_register_all();

	//show_banner();
	unsigned version = avcodec_version();
	printf("version:%d\n", version);

	// 初始化网络库
	avformat_network_init();

	printf("================> before init rtsp\n");
	init_rtsp();
	printf("================> after init rtsp\n");

	while(is_start){
		usleep(50*1000);
	}

	deinit_rtsp();

	return 0;
}
