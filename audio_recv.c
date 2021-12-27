/******************************************************************************
 *
 *       Filename:  audio_recv.c
 *
 *    Description:  audio recv
 *
 *        Version:  1.0
 *        Created:  2021年12月14日 16时08分35秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
/*!
* \file audio_recv.c
* \brief 接收rtsp音频文件
* 
* 
* \author yangkun
* \version 1.0
* \date 2021.12.14
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

typedef struct  s_audio_recv
{
	char url[512];    //存储url

	AVFormatContext *ifmt_ctx ;   //  rtsp 接收
	AVCodecContext *dec_ctx; // 音频解码
	int audioindex;
	int videoindex;

	struct SwrContext *swr_ctx; // 格式转换

	pthread_t th_recv;
	int is_start;

	audio_recv_cb audio_cb;
	long audio_handle; // 音频回调句柄

}t_audio_recv;


// 使用ffmpeg 全局初始化
int ffmpeg_init()
{
	av_log_set_level(AV_LOG_VERBOSE);
    
	av_register_all();
	avfilter_register_all();

	//show_banner();
	unsigned version = avcodec_version();
	printf("version:%d\n", version);

	// 初始化网络库
	avformat_network_init();
	av_register_all();
	avfilter_register_all();
	avdevice_register_all();


	return 0;
}

// 音频数据接收回调
void *audio_recv_proc(void *arg)

{
	if(!arg) {
		printf("!!error in audio_recv proc handle!!\n");
		exit(1);
	}
	t_audio_recv *p_audio_recv = (t_audio_recv *)arg;

	AVFormatContext *ifmt_ctx = NULL;
	struct SwrContext *swr_ctx = NULL;
	AVCodecContext *dec_ctx = NULL;

	ifmt_ctx = p_audio_recv->ifmt_ctx;
	swr_ctx = p_audio_recv->swr_ctx;
	dec_ctx = p_audio_recv->dec_ctx;
	assert(ifmt_ctx);
	assert(swr_ctx);
	assert(dec_ctx);

	int ret;
	AVPacket pkt;
    AVFrame *frame = av_frame_alloc();

	uint8_t **dst_data = NULL;
	int dst_nb_samples;
	int dst_nb_channels =0;
	int dst_ch_layout = AV_CH_LAYOUT_STEREO;
	enum AVSampleFormat dst_sample_fmt = AV_SAMPLE_FMT_S16;
	int dst_linesize = 0;


	int frame_samples = 1024;
	int src_rate = 48000, dst_rate = 48000;
	dst_nb_samples = av_rescale_rnd(frame_samples, dst_rate, src_rate, AV_ROUND_UP);
	// 源和目的采样率相同，采样点应该也是相同的
	assert(dst_nb_samples = frame_samples);

	/* buffer is going to be directly written to a rawaudio file, no alignment */
	int max_dst_nb_samples = dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);
	assert(dst_nb_channels == 2);
	// 根据格式分配内存
	ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,
			dst_nb_samples, dst_sample_fmt, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate destination samples\n");
		goto end;
	}


	while(p_audio_recv->is_start) {
		ret = av_read_frame(ifmt_ctx, &pkt);
		if(ret < 0) {
			printf("error to read frame\n");
			break;
		}

		// 解码音频
		if(pkt.stream_index == p_audio_recv->audioindex) {
			ret = avcodec_send_packet(dec_ctx, &pkt);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
				break;
			}

			while (ret >= 0) {
				ret = avcodec_receive_frame(dec_ctx, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					break;
				} else if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
					goto end;
				}

				if (ret >= 0) {
					int frame_size = frame->nb_samples;

					/* compute destination number of samples */
					dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, src_rate) +
							frame_size, dst_rate, src_rate, AV_ROUND_UP);
					if (dst_nb_samples > max_dst_nb_samples) {
						av_freep(&dst_data[0]);
						ret = av_samples_alloc(dst_data, &dst_linesize, dst_nb_channels,
								dst_nb_samples, dst_sample_fmt, 1);
						if (ret < 0)
							break;
						max_dst_nb_samples = dst_nb_samples;
						printf("=========================> dst_nb_samples !! change!!\n");
					}	

					// printf("frame_size:%d dst_nb_samples:%d\n", frame_size, dst_nb_samples);
					//  先转换为48000 s16格式
					/* convert to destination format */
					ret = swr_convert(swr_ctx, dst_data, dst_nb_samples, (const uint8_t **)frame->data, frame_size);
					if (ret < 0) {
						fprintf(stderr, "Error while converting\n");
						goto end;
					}

					assert(dst_data != NULL);
					//int size = frame_size*2*2;
					int size = frame_size;
					//if(av_audio_fifo_write(fifo1, (void **)dst_data, size) < size)
					assert(p_audio_recv);

					// 将音频数据回调送出去
					// printf("handle:%lld\n", p_audio_recv->audio_handle);

					p_audio_recv->audio_cb((void*)dst_data, size, p_audio_recv->audio_handle);
					assert(p_audio_recv->audio_handle != 0);


					av_frame_unref(frame);
				}
			}	
		}

		//释放内存
		av_free_packet(&pkt);
		//av_packet_unref(&pkt);
	}

end:
	printf("rtsp audio recv will exit\n");
	// ？这句话有用吗？没有这句话，会检测到有内存泄漏
	av_free_packet(&pkt);
	av_frame_free(&frame);

	if (dst_data)
		av_freep(&dst_data[0]);
	av_freep(&dst_data);



}

int audio_recv_create(FHANDLE *hd, const char *url, audio_recv_cb _cb, long handle)
{
	int ret;

	AVFormatContext *ifmt_ctx = NULL;   //  rtsp 接收
	AVCodecContext *dec_ctx = NULL; // 音频解码
	int audioindex = 0;
	int videoindex = 0;

	t_audio_recv *p_audio_recv = (t_audio_recv *)malloc(sizeof(t_audio_recv));
	memset(p_audio_recv, 0, sizeof(t_audio_recv));
	if(!p_audio_recv ) 
	{
		printf("ERROR to malloc t_audio_recv memory:%d:%s\n", errno, strerror(errno));
		exit(1);
	}


	if(!url) {
			printf("error in url !!\n");
			free(p_audio_recv);
			return -1;
	}

	if(!_cb)
	{
		printf("ERROR to  get cb\n");
		free(p_audio_recv);
		return -2;
	}


	strcpy(p_audio_recv->url, url);
	p_audio_recv->audio_cb = _cb;
	p_audio_recv->audio_handle =  handle;


	{
		// 设置超时等属性
		AVDictionary* options = NULL;
#if 1
		av_dict_set(&options, "rtsp_transport", "tcp", 0);
		av_dict_set(&options, "stimeout", "1000000", 0); //没有用

		ifmt_ctx = avformat_alloc_context();

		// 设置超时回调函数后上面的超时属性才生效, 可以在超时函数中处理其他逻辑
		//AVIOInterruptCB icb = {interruptcb, NULL};
		//ifmt_ctx1->interrupt_callback = icb;
#endif

		// 记录当前时间
		//gettimeofday(&start_time, NULL);

		// 初始化输入
		if ((ret = avformat_open_input(&ifmt_ctx, p_audio_recv->url, 0, &options)) < 0) {
			printf( "Could not open input file.");
			av_dict_free(&options);
			ret = -2;
			goto end;
		}
		av_dict_free(&options);
		// 连接成功后中断中不检查了
		//_out = 1;

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

		printf("url:%s dump format:\n", p_audio_recv->url);
		// 打印流媒体信息
		av_dump_format(ifmt_ctx, 0, p_audio_recv->url, 0);

		AVCodec *dec;
		/* select the audio stream */
		ret = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot find an audio stream in the input file\n");
			ret = -2;
			goto end;
		}
		audioindex = ret;


		dec_ctx = avcodec_alloc_context3(dec);
		if (!dec_ctx)
			return AVERROR(ENOMEM);
		avcodec_parameters_to_context(dec_ctx, ifmt_ctx->streams[audioindex]->codecpar);

		/* init the audio decoder */
		if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot open audio decoder\n");
			return ret;
		}
	}

	assert(ifmt_ctx);
	p_audio_recv->dec_ctx = dec_ctx;
	p_audio_recv->ifmt_ctx = ifmt_ctx;
	p_audio_recv->audioindex = audioindex;
	p_audio_recv->videoindex = videoindex;

	{

		static struct SwrContext *swr_ctx = NULL;
		int64_t src_ch_layout = AV_CH_LAYOUT_STEREO, dst_ch_layout = AV_CH_LAYOUT_STEREO;
		int src_rate = 48000, dst_rate = 48000;
		enum AVSampleFormat src_sample_fmt = AV_SAMPLE_FMT_FLTP, dst_sample_fmt = AV_SAMPLE_FMT_S16;
		/* create resampler context */
		swr_ctx = swr_alloc();
		if (!swr_ctx) {
			fprintf(stderr, "Could not allocate resampler context\n");
			ret = AVERROR(ENOMEM);
			//goto end;
			exit(1);
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
		p_audio_recv->swr_ctx = swr_ctx;
	}



	p_audio_recv->is_start = 1;
#if 1
	// 创建线程接收音频数据
	ret = pthread_create(&p_audio_recv->th_recv, NULL, audio_recv_proc, (void*)p_audio_recv);
	if(ret != 0){
		printf("error to create thread:errno = %d\n", errno);
		ret = -6;
		exit(1);
		goto end;
	}
#endif


	// 将handle传递出去
	*hd = (void*)p_audio_recv;

	return 0;
end:
	avformat_free_context(ifmt_ctx);
	avformat_close_input(&ifmt_ctx);
	ifmt_ctx = NULL;

	return ret;
}


int audio_recv_remove(FHANDLE *hd)
{

	printf("==========================>%d\n", __LINE__);
	t_audio_recv *p_audio_recv = (t_audio_recv *)(*hd);
	if(!hd) {
		printf("error TO get handle \n");
		return -1;
	}

	// 等待线程退出
#if 1
	p_audio_recv->is_start = 0;
	pthread_join(p_audio_recv->th_recv, NULL);
#endif


	if(p_audio_recv->ifmt_ctx) {
		avformat_close_input(&p_audio_recv->ifmt_ctx);
		avformat_free_context(p_audio_recv->ifmt_ctx);
		p_audio_recv->ifmt_ctx = NULL;
	}

	avcodec_close(p_audio_recv->dec_ctx);
	avcodec_free_context(&p_audio_recv->dec_ctx);
	
	if(p_audio_recv->swr_ctx)
	{
		swr_free(&p_audio_recv->swr_ctx);
	}

	free(p_audio_recv);
	p_audio_recv = NULL;
	*hd = NULL;

	printf("==========================>%d\n", __LINE__);

	return 0;
}
