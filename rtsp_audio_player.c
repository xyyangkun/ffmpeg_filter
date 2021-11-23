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
 *   ./rtsp_audio_player |ffplay -f s16le -ar 48000 -ac 2 -
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
//#include <libavfilter/avfiltergraph.h>

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
#include <sys/time.h>
#include <assert.h>

#define INPUT_SAMPLERATE     48000
#define INPUT_FORMAT         AV_SAMPLE_FMT_S16
#define INPUT_CHANNEL_LAYOUT AV_CH_LAYOUT_STEREO

/** The output bit rate in kbit/s */
#define OUTPUT_BIT_RATE 48000
/** The number of output channels */
#define OUTPUT_CHANNELS 2
/** The audio sample output format */
#define OUTPUT_SAMPLE_FORMAT AV_SAMPLE_FMT_S16

#define VOLUME_VAL 80


//AVFormatContext *output_format_context = NULL;
//AVCodecContext *output_codec_context = NULL;

//AVFormatContext *input_format_context_0 = NULL;
AVCodecContext *input_codec_context_0 = NULL;

AVFilterGraph *graph;
AVFilterContext *src0, *sink, *_volume;

static char *const get_error_text(const int error)
{
    static char error_buffer[255];
    av_strerror(error, error_buffer, sizeof(error_buffer));
    return error_buffer;
}

static int init_filter_graph(AVFilterGraph **graph, AVFilterContext **src0, 
                             AVFilterContext **sink)
{
    AVFilterGraph *filter_graph;
	AVFilterContext *abuffer0_ctx;
    const AVFilter        *abuffer0;
    AVFilterContext *volume_ctx;
    const AVFilter        *volume_filter;
    AVFilterContext *abuffersink_ctx;
    const AVFilter        *abuffersink;
	
	char args[512];
	
    int err;
	
    /* Create a new filtergraph, which will contain all the filters. */
    filter_graph = avfilter_graph_alloc();
    if (!filter_graph) {
        av_log(NULL, AV_LOG_ERROR, "Unable to create filter graph.\n");
        return AVERROR(ENOMEM);
    } /****** abuffer 0 ********/ /* Create the abuffer filter;
     * it will be used for feeding the data into the graph. */
    abuffer0 = avfilter_get_by_name("abuffer");
    if (!abuffer0) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the abuffer filter.\n");
        return AVERROR_FILTER_NOT_FOUND;
    }
	
	/* buffer audio source: the decoded frames from the decoder will be inserted here. */
    if (!input_codec_context_0->channel_layout)
        input_codec_context_0->channel_layout = av_get_default_channel_layout(input_codec_context_0->channels);
    snprintf(args, sizeof(args),
			 "sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
             input_codec_context_0->sample_rate,
             av_get_sample_fmt_name(input_codec_context_0->sample_fmt), input_codec_context_0->channel_layout);

	printf("filter input args:%s\n", args);
	
	
	err = avfilter_graph_create_filter(&abuffer0_ctx, abuffer0, "src",
                                       args, NULL, filter_graph);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        return err;
    }
    



    /****** volume ******* */
    /* Create volume filter. */
    volume_filter = avfilter_get_by_name("volume");
    if (!volume_filter) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the mix filter.\n");
        return AVERROR_FILTER_NOT_FOUND;
    }
    
    // snprintf(args, sizeof(args), "volume=0.1");
    //snprintf(args, sizeof(args), "volume=0.1");
    snprintf(args, sizeof(args), "volume=2");
	
	err = avfilter_graph_create_filter(&volume_ctx, volume_filter, "volume_sink",
                                       args, NULL, filter_graph);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio amix filter\n");
        return err;
    }

	_volume = volume_ctx;

	{
		uint8_t *volume_str = NULL;
		av_opt_get(_volume->priv, "volume", 0, &volume_str);
		printf("========================>get volume:%s\n", volume_str);
		av_freep(&volume_str);
	}
	
    /* Finally create the abuffersink filter;
     * it will be used to get the filtered data out of the graph. */
    abuffersink = avfilter_get_by_name("abuffersink");
    if (!abuffersink) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the abuffersink filter.\n");
        return AVERROR_FILTER_NOT_FOUND;
    }
	
    abuffersink_ctx = avfilter_graph_alloc_filter(filter_graph, abuffersink, "sink");
    if (!abuffersink_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate the abuffersink instance.\n");
        return AVERROR(ENOMEM);
    }
	
    /* Same sample fmts as the output file. */
    err = av_opt_set_int_list(abuffersink_ctx, "sample_fmts",
                              ((int[]){ AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_NONE }),
                              AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    
    uint8_t ch_layout[64];
    av_get_channel_layout_string(ch_layout, sizeof(ch_layout), 0, OUTPUT_CHANNELS);
    av_opt_set    (abuffersink_ctx, "channel_layout", ch_layout, AV_OPT_SEARCH_CHILDREN);
    
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could set options to the abuffersink instance.\n");
        return err;
    }
    
    err = avfilter_init_str(abuffersink_ctx, NULL);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not initialize the abuffersink instance.\n");
        return err;
    }
    
    
    /* Connect the filters; */
    
	err = avfilter_link(abuffer0_ctx, 0, volume_ctx, 0);
	if (err >= 0)
        err = avfilter_link(volume_ctx, 0, abuffersink_ctx, 0);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error connecting filters\n");
        return err;
    }
	
    /* Configure the graph. */
    err = avfilter_graph_config(filter_graph, NULL);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error while configuring graph : %s\n", get_error_text(err));
        return err;
    }

#if 0
    char* dump =avfilter_graph_dump(filter_graph, NULL);
    av_log(NULL, AV_LOG_ERROR, "Graph :\n%s\n", dump);
	av_free(dump);
#endif
	
    *graph = filter_graph;
    *src0   = abuffer0_ctx;
    *sink  = abuffersink_ctx;
    
    return 0;
}



// rtsp 回调，连接rtsp成功后，接收server端发来的音视频数据
// type = 0 视频 1 音频
typedef void (*rtsp_cb)(void *buf, int len, int time, int type, long param);

// const char *str = "0.1";
// const char *str = "5";
// =0 mute
void set_volume(const char *str){
	int ret;
#if 0
	av_opt_set(_volume->priv, "volume", AV_STRINGIFY("volume=0.1"), 0);
#else
	// 通过avfilter_graph_send_command 实现
	const char *target = "volume_sink";
	const char *cmd = "volume";
	const char *arg = str;
	char *res=NULL;
	int res_len=0;
	int flags=0;
	ret = avfilter_graph_send_command(graph, target, cmd, arg, res, res_len, flags);
	if(ret < 0) {
		printf("graph send comman error!!\n");
	}

#endif
}

//https://blog.csdn.net/garefield/article/details/86491448
//参考动态修改
//
int is_start = 0;
static void sigterm_handler(int sig) {
	//printf("=========>get sig:%d %d\n", sig, SIGUSR1);

	static int ch = 0;
	// 收到这个信号改变音量
	// SIGUSR1 == 10 要先注册才能使用
	if(sig == SIGUSR1) {
	//if(sig == 10) {
		if(0){
			uint8_t *volume_str = NULL;
			av_opt_get(_volume->priv, "volume", 0, &volume_str);
			printf("========================>before get sigterm volume:%s\n", volume_str);
			av_freep(&volume_str);
		}

		//AVDictionary *options_dict = NULL;
		if(ch == 0){
			ch = 1;
			const char *str = "0";
			set_volume(str);
			//printf("volume = 0.1\n");
		}else {
			ch = 0;
			const char *str = "2";
			set_volume(str);
			//printf("volume = 10\n");
		}
		//av_dict_set(&options_dict, "volume", AV_STRINGIFY(VOLUME_VAL), 0);
		if(0){
			uint8_t *volume_str = NULL;
			av_opt_get(_volume->priv, "volume", 0, &volume_str);
			printf("========================>get sigterm volume:%s\n", volume_str);
			av_freep(&volume_str);
		}


#if 0
		printf("debug %d  ==>%p\n", __LINE__, graph);
		int err = avfilter_init_dict(graph, &options_dict);
		av_dict_free(&options_dict);
		printf("debug %d\n", __LINE__);
		if (err < 0) {
			fprintf(stderr, "Could not initialize the volume filter.\n");
			return err;
		}

		printf("debug %d\n", __LINE__);
		printf("retuan\n");
#endif
		return;
	}

	is_start = 0;
}

static AVCodecContext *dec_ctx;


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
			// 解码音频
		}
	}
}

static void print_frame(const AVFrame *frame)
{
    const int n = frame->nb_samples * av_get_channel_layout_nb_channels(frame->channel_layout);
    const uint16_t *p     = (uint16_t*)frame->data[0];
    const uint16_t *p_end = p + n;

    while (p < p_end) {
        fputc(*p    & 0xff, stdout);
        fputc(*p>>8 & 0xff, stdout);
        p++;
    }
    fflush(stdout);
}


void *recv_proc(void *param)
{
	int ret;
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


	AVPacket pkt;
    AVFrame *frame = av_frame_alloc();
    AVFrame *filt_frame = av_frame_alloc();

	while(is_start) {
		ret = av_read_frame(ifmt_ctx, &pkt);
		if(ret < 0) {
			printf("error to read frame\n");
			break;
		}

		// 等待filter 初始化完成
		if(src0 == NULL){
			av_packet_unref(&pkt);
			continue;
		}

		// 暂时不处理视频
#if 0
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
#endif
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


		
		// 解码音频
		if(pkt.stream_index == audioindex) {
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
					/* push the audio data from decoded frame into the filtergraph */
					ret = av_buffersrc_add_frame_flags(src0, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
					//ret = av_buffersrc_write_frame(src0, frame);
					if (ret < 0) {
						av_log(NULL, AV_LOG_ERROR, "Error while feeding the audio filtergraph\n");
						break;
					}

					/* pull filtered audio from the filtergraph */
					while (1) {
						ret = av_buffersink_get_frame(sink, filt_frame);
						if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
							break;
						if (ret < 0)
							goto end;
						//print_frame(filt_frame);
						//printf("filter_frame size:%d\n", filt_frame->pkt_size);
						av_frame_unref(filt_frame);
					}
					av_frame_unref(frame);
				}
			}	
		}

		//释放内存
		//av_free_packet(&pkt);
		av_packet_unref(&pkt);
	}

end:
	//av_free_packet(&pkt);
	av_frame_free(&frame);
	av_frame_free(&filt_frame);
return NULL;

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
	av_dict_set(&options, "max_delay", "5000000", 0); //设置最大延迟时间

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

	// 老的代码
#if 0
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
#endif

	printf("will demp format:\n");
	// 打印流媒体信息
	av_dump_format(ifmt_ctx, 0, in_filename, 0);

	AVCodec *dec;
	/* select the audio stream */
	ret = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find an audio stream in the input file\n");
		return ret;
	}
	int audio_stream_index = ret;


	dec_ctx = avcodec_alloc_context3(dec);
	if (!dec_ctx)
		return AVERROR(ENOMEM);
	avcodec_parameters_to_context(dec_ctx, ifmt_ctx->streams[audio_stream_index]->codecpar);

	/* init the audio decoder */
	if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open audio decoder\n");
		return ret;
	}




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

	printf("=======================>%d\n", __LINE__);
	is_start = 0;
	if(recv_id!=0) {
		pthread_join(recv_id, NULL);
		recv_id = 0;
	}

	printf("=======================>%d\n", __LINE__);

	if(ifmt_ctx) {
		avformat_close_input(&ifmt_ctx);
		avformat_free_context(ifmt_ctx);
		ifmt_ctx = NULL;
	}
	printf("=======================>%d\n", __LINE__);

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
	signal(SIGUSR1, sigterm_handler);

    av_log_set_level(AV_LOG_VERBOSE);
    

    int err;
	
	//av_register_all();
	//avfilter_register_all();

	//show_banner();
	unsigned version = avcodec_version();
	printf("version:%d\n", version);

	// 初始化网络库
	avformat_network_init();

	printf("================> before init rtsp\n");
	init_rtsp();
	printf("================> after init rtsp\n");

	input_codec_context_0 = dec_ctx;

   /* Set up the filtergraph. */
    err = init_filter_graph(&graph, &src0, &sink);
	assert(src0 != NULL);
	printf("Init err = %d\n", err);


	while(is_start){
		usleep(50*1000);
	}

	deinit_rtsp();

	printf("========> FINISHED\n");

	avcodec_close(dec_ctx);;
	avcodec_free_context(&dec_ctx);

	avfilter_graph_free(&graph);

    

	return 0;
}
