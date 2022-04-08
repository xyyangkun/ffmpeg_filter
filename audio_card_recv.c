/******************************************************************************
 *
 *       Filename:  audio_card_recv.c
 *
 *    Description:  audio card recv
 *
 *        Version:  1.0
 *        Created:  2021年12月15日 14时08分32秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
/*!
* \file audio_card_recv.c
* \brief 接收 声卡数据
* 
* 
* \author yangkun
* \version 1.0
* \date 2021.12.15
*/
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
#if 0
#include <libavfilter/avfiltergraph.h>
#else

#endif

#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>

#include "libavdevice/avdevice.h"

#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

#include "sound_card_get.h"

#include "multi_audio_mix.h"

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>

#include "audio_recv.h"
#include "audio_card_recv.h"

//#define OUTPUT_CHANNELS 2
//int usb_audio_channels = 2;                       // 1
//int card_audio_sample_rate = 16000;
//int usb_audio_channel_layout = AV_CH_LAYOUT_MONO; // AV_CH_LAYOUT_STEREO
//static int usb_frame_size = 128;
//static int usb_frame_size_trans = 128;

//const int alsa_out_samples = 1024;


/**
 * @brief 声卡数据接收结构
 */
typedef struct s_audio_card_recv
{
	AVInputFormat *ifmt;

	AVFormatContext *ifmt_ctx;

	int audio_sample_rate; //  声卡采样率 默认48000 usb可能是16000
	int audio_channels;     // 1 单声道 2 立体声
	int audio_channel_layout; // AV_CH_LAYOUT_MONO; // AV_CH_LAYOUT_STEREO
	int alsa_out_samples; // 1024 每包读取数据点数

	struct SwrContext *swr_ctx; // 格式转换

	pthread_t th_audio_card_recv; /// 声卡接收线程
	int sound_card_is_start;      /// 声卡是否开始接收数据

	// 回调
	audio_card_recv_cb cb;
	long audio_recv_handle; // 回调handle
}t_audio_card_recv;

static char *const get_error_text(const int error)
{
    static char error_buffer[255];
    av_strerror(error, error_buffer, sizeof(error_buffer));
    return error_buffer;
}

static void *audio_card_recv_proc(void *param)
{
	t_audio_card_recv *p_audio_card_recv = (t_audio_card_recv *)param;
	if(p_audio_card_recv == NULL) 
	{
		printf("error in get audio_card_recv_proc param\n");
		exit(1);
	}

	int ret;
	AVPacket pkt;

	int frame_size =0;

#if 1
    //AVFrame *frame = av_frame_alloc();
#endif
	AVFrame *_frame;

	uint8_t **dst_data = NULL;
	int dst_nb_samples;
	int dst_nb_channels =0;
	int dst_ch_layout = AV_CH_LAYOUT_STEREO;
	enum AVSampleFormat dst_sample_fmt = AV_SAMPLE_FMT_S16;
	int dst_linesize = 0;


	// int frame_samples = usb_frame_size;// 1024;
	int frame_samples = p_audio_card_recv->alsa_out_samples;
	printf("frame_samples==>%d\n", frame_samples);
	int src_rate = p_audio_card_recv->audio_sample_rate;
	assert(p_audio_card_recv->audio_sample_rate != 0);
	int dst_rate = 48000;
	int max_dst_nb_samples = dst_nb_samples = av_rescale_rnd(frame_samples, dst_rate, src_rate, AV_ROUND_UP);
	// 源和目的采样率相同，采样点应该也是相同的
	//assert(dst_nb_samples = frame_samples);

	/* buffer is going to be directly written to a rawaudio file, no alignment */
	dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);

	assert(dst_nb_channels == 2);
	// 根据格式分配内存
	ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,
			dst_nb_samples, dst_sample_fmt, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate destination samples\n");
		goto end;
	}

	printf("init ============================> frame_samples:%d dst_nb_samples =%d\n", frame_samples, dst_nb_samples);


	while(p_audio_card_recv->sound_card_is_start) {
		// usb camera接收数据帧大小固定的
		ret = av_read_frame(p_audio_card_recv->ifmt_ctx, &pkt);
		if(ret < 0) {
			printf("11error to read frame:%d %d, %d, sound_card_is_start=%d\n",
					ret, errno, EAGAIN, p_audio_card_recv->sound_card_is_start);
			perror("read:");
			//if(errno == EAGAIN)
			if(ret == -EAGAIN)
			{
				usleep(20000);
				continue;
			}
			break;
		}

		//av_log(NULL, AV_LOG_DEBUG, "packet pkt size:%d\n", pkt.size);
		//printf("packet pkt size:%d\n", pkt.size);
		//cb(pkt.data, pkt.size, 0, 1, param);
		{


#if 1
			// 可以直接从package包中获取数据，然后生成AVFrame 包，将数据放入AVFrame包中
			int pkt_size = pkt.size;
			_frame = av_frame_alloc();

			_frame->nb_samples     = pkt_size/2/p_audio_card_recv->audio_channels;
			//_frame->channel_layout = av_get_default_channel_layout(AV_CH_LAYOUT_STEREO);
			_frame->channel_layout = AV_CH_LAYOUT_STEREO;
			_frame->format         = AV_SAMPLE_FMT_S16;
			//_frame->sample_rate    = 16000;
			_frame->sample_rate    = p_audio_card_recv->audio_sample_rate;

			if ((ret = av_frame_get_buffer(_frame, 0)) < 0) {
				fprintf(stderr, "Could not allocate output frame samples (error '%s')\n",
						av_err2str(ret));
				av_frame_free(&_frame);
				//return ret;
				exit(1);
			}
			av_frame_make_writable (_frame);
			av_frame_set_pkt_size(_frame, pkt_size);


			int frame_size = _frame->nb_samples;
			//printf("usb ====> frame_size:%d\n", frame_size);
			//printf("usb ====> nb_samples:%d\n", frame->nb_samples);
			//printf("usb ====>  pkt_size:%d %d\n", frame->pkt_size, pkt_size);
			memcpy(_frame->data[0], pkt.data, pkt_size);

			assert(pkt_size> 0);
			assert(_frame->nb_samples> 0);
#if 1
			// 转换成48k双通道后入栈

			/* compute destination number of samples */
			dst_nb_samples = av_rescale_rnd(swr_get_delay(p_audio_card_recv->swr_ctx, src_rate) +
					frame_size, dst_rate, src_rate, AV_ROUND_UP);
			if (dst_nb_samples > max_dst_nb_samples) {
				av_freep(&dst_data[0]);
				ret = av_samples_alloc(dst_data, &dst_linesize, dst_nb_channels,
						dst_nb_samples, dst_sample_fmt, 1);
				if (ret < 0)
					break;
				max_dst_nb_samples = dst_nb_samples;
				printf("=========================> !! change!!\n");
			}	

			//  先转换为48000 s16格式
			/* convert to destination format */
			ret = swr_convert(p_audio_card_recv->swr_ctx, dst_data, dst_nb_samples, (const uint8_t **)&pkt.data, frame_size);
			if (ret < 0) {
				fprintf(stderr, "Error while converting\n");
				goto end;
			}
			int dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,
					ret, dst_sample_fmt, 1);
			if (dst_bufsize < 0) {
				fprintf(stderr, "Could not get sample buffer size\n");
				goto end;
			}

#if 0
			printf("usb frame_size:%d dst_nb_samples:%d, dst_linesize:%d, dst_bufsize:%d\n",
				 frame_size, dst_nb_samples, dst_linesize, dst_bufsize);
#endif
			
			// 双通道立体声
			int size = dst_bufsize/4;
			//usb_frame_size_trans = size;
		
#if 0
			pthread_mutex_lock(&counter_mutex3);
			//if(av_audio_fifo_write(fifo3, (void **)dst_data, frame_size) < frame_size)
			if(av_audio_fifo_write(fifo3, (void **)dst_data, size) < size)
			{
				printf("fifo size:%d\n", av_audio_fifo_size(fifo3));
				fprintf(stderr, "cound not write data to fifo\n");
				exit(1);
			}
			pthread_mutex_unlock(&counter_mutex3);
#endif
			// 将数据通过回调传出去
			if(p_audio_card_recv->cb)
			{
				//printf("usb card will call cb, size:%d \n", size);
				p_audio_card_recv->cb(dst_data, size, p_audio_card_recv->audio_recv_handle);
			}


#else
#endif


			//av_frame_unref(frame);
			av_frame_free(&_frame);
#endif
		}

		//av_free_packet(&pkt);
		av_packet_unref(&pkt);
	}


end:
	printf("card recv proc will exit\n");
	if (dst_data)
		av_freep(&dst_data[0]);
	av_freep(&dst_data);

	//av_frame_free(&frame);
	av_frame_free(&_frame);

	av_log(NULL, AV_LOG_INFO, "recv proc exit\n");

	// ？这句话有用吗？没有这句话，会检测到有内存泄漏
	//av_free_packet(&pkt);
	av_packet_unref(&pkt);

}


int audio_card_recv_create(FHANDLE *hd, const char *name, sound_card_info *info,  audio_card_recv_cb _cb, long handle)
{
	// 分配声卡内存
	t_audio_card_recv *p_audio_card_recv = (t_audio_card_recv *)malloc(sizeof(t_audio_card_recv));
	if(!p_audio_card_recv) 
	{
		printf("ERROR in malloc t_audio_card_recv!\n");
		exit(1);
	}
	memset(p_audio_card_recv, 0, sizeof(sizeof(t_audio_card_recv)));
	p_audio_card_recv->cb = _cb;
	p_audio_card_recv->audio_recv_handle = handle;

	// 配置声卡信息
	char channel[100] = {0};
	p_audio_card_recv->audio_channels = info->channels;                       // 1
	p_audio_card_recv->audio_sample_rate = info->sample;
	printf("=========================> audio_sample_rate = %d\n", p_audio_card_recv->audio_sample_rate);
	if (p_audio_card_recv->audio_channels == 1) {
		p_audio_card_recv->audio_channel_layout = AV_CH_LAYOUT_MONO; // AV_CH_LAYOUT_STEREO
		sprintf(channel, "%d", 1);
		printf("===========================> AV_CH_LAYOUT_STEREO\n");
	}
	else {
		p_audio_card_recv->audio_channel_layout = AV_CH_LAYOUT_STEREO;
		sprintf(channel, "%d", 2);
		printf("==========================> AV_CH_LAYOUT_STEREO\n");
	}
	p_audio_card_recv->alsa_out_samples = 1024;


	AVInputFormat *ifmt;

	struct SwrContext *swr_ctx = NULL;

	AVFormatContext *ifmt_ctx = NULL;

	int ret;
	ifmt = av_find_input_format("alsa");
	if(ifmt == NULL) {
		perror("error to found alsa");
		assert(1);
		return -1;
	}
	assert(ifmt);

	ifmt_ctx = avformat_alloc_context(); 
	assert(ifmt_ctx);

	// 查看声卡信息：
	// cat /proc/asound/cards
	// arecord -l
	// 捕获声卡数据
	// -loglevel trace
	// sudo ffmpeg -f alsa -channels 1 -i hw:3 -t 30 out.wav
	// arm: ffmpeg -f alsa -channels 2 -i hw:1 -t 30 out.wav
	// dump 声卡参数
	// arecord -D hw:3 --dump-hw-params
	//const char *in_filename = "hw:3,0";
	//const char *in_filename = "hw:1,0";
	//const char *in_filename = "default";
	// usb 声卡可以增加采样点数目-thread_queue_size 1024
	AVDictionary * options = NULL;
	//av_dict_set(&options, "channels", "2", 0);
	//av_dict_set(&options, "channels", "2", 0);
	av_dict_set(&options, "channels", channel, 0);
	//av_dict_set(&options, "sample_rate", "32000", 0);
	// 可能需要编译 alsa 支持的ffmpeg dev 库
	//av_dict_set(&options, "sample_rate", "16000", 0);
	// av_dict_set_int(&options, "sample_rate", 16000, 0);
	av_dict_set_int(&options, "sample_rate", p_audio_card_recv->audio_sample_rate, 0);

	//av_dict_set(&options, "thread_queue_size", "1024", 0);
	
	ifmt_ctx->flags |= AVFMT_FLAG_NONBLOCK;

	if(avformat_open_input(&ifmt_ctx, name, ifmt, &options)!=0){  
		perror("Couldn't open input audio stream.\n");  
		assert(1);
		av_dict_free(&options);
		return -1;  
	}
	av_dict_free(&options);

	AVCodec *dec;
	/* select the audio stream */
	ret = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find an audio stream in the input file\n");
		return ret;
	}
	int audio_index = ret;

	printf("will demp format:\n");
	// 打印流媒体信息
	av_dump_format(ifmt_ctx, 0, name, 0);

    /** Open the decoder for the audio stream to use it later. */
    if ((ret = avcodec_open2(ifmt_ctx->streams[0]->codec,
                               dec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open input codec (error '%s')\n",
               get_error_text(ret));
        avformat_close_input(&ifmt_ctx);
		exit(1);
    }
	
    //input_codec_context = ifmt_ctx->streams[0]->codec;
	//dec_ctx = ifmt_ctx->streams[0]->codec;



	// 创建转换
	// audio usb
	{
		int64_t src_ch_layout = AV_CH_LAYOUT_STEREO, dst_ch_layout = AV_CH_LAYOUT_STEREO;
		int src_rate = 48000, dst_rate = 48000;
		enum AVSampleFormat src_sample_fmt = AV_SAMPLE_FMT_S16, dst_sample_fmt = AV_SAMPLE_FMT_S16;

		// 动态获取usb格式输入
		if(p_audio_card_recv->audio_channels == 2)
			src_ch_layout = AV_CH_LAYOUT_STEREO;
		else 
			src_ch_layout = AV_CH_LAYOUT_MONO;
		src_rate = p_audio_card_recv->audio_sample_rate;
		// src_sample_fmt = AV_SAMPLE_FMT_S16;  // 暂时不支持除S16以外的格式

		/* create resampler context */
		swr_ctx = swr_alloc();
		if (!swr_ctx) {
			fprintf(stderr, "Could not allocate resampler context\n");
			ret = AVERROR(ENOMEM);
			ret = -1;
			goto end;
		}

		/* set options */
		av_opt_set_int(swr_ctx, "in_channel_layout",    src_ch_layout, 0);
		av_opt_set_int(swr_ctx, "in_sample_rate",       src_rate, 0);
		av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", src_sample_fmt, 0);

		av_opt_set_int(swr_ctx, "out_channel_layout",    dst_ch_layout, 0);
		av_opt_set_int(swr_ctx, "out_sample_rate",       dst_rate, 0);
		av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);

		/* initialize the resampling context */
		if ((ret = swr_init(swr_ctx)) < 0) {
			fprintf(stderr, "Failed to initialize the resampling context\n");
			ret = -1;
			goto end;
		}
	}



	// 赋值handle
	p_audio_card_recv->swr_ctx = swr_ctx;	
	p_audio_card_recv->ifmt_ctx = ifmt_ctx;
	*hd = p_audio_card_recv;

	p_audio_card_recv->sound_card_is_start = 1;
	// 创建线程接收数据
	ret = pthread_create(&p_audio_card_recv->th_audio_card_recv, NULL, audio_card_recv_proc, (void *)p_audio_card_recv);
	if(ret != 0){
		printf("error to create thread:errno = %d\n", errno);
		exit(1);
	}

	return 0;
end:
	avformat_free_context(ifmt_ctx);
	avformat_close_input(&ifmt_ctx);
	ifmt_ctx = NULL;
	return ret;
}

int audio_card_recv_remove(FHANDLE *hd)
{
	t_audio_card_recv *p_audio_card_recv = (t_audio_card_recv *)(*hd);
	if(p_audio_card_recv == NULL) 
	{
		printf("error in get audio_card_recv_proc param\n");
		//exit(1);
		return -1;
	}

	// 停止线程
	p_audio_card_recv->sound_card_is_start = 0;
	if(p_audio_card_recv->th_audio_card_recv)
	{
		pthread_join(p_audio_card_recv->th_audio_card_recv, NULL);
		p_audio_card_recv->th_audio_card_recv = 0;
	}

	// 释放
	avformat_close_input(&p_audio_card_recv->ifmt_ctx); 
	avformat_free_context(p_audio_card_recv->ifmt_ctx);


	swr_free(&p_audio_card_recv->swr_ctx);


	// 释放内存
	free(p_audio_card_recv);
	*hd = NULL;


	return 0;

}
