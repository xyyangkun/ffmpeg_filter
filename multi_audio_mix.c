/******************************************************************************
 *
 *       Filename:  multi_audio_mix.c
 *
 *    Description:  multi audio mix 
 *
 *        Version:  1.0
 *        Created:  2021年11月23日 17时35分09秒
 *       Revision:  none
 *       Compiler:  gcc
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

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>

#define OUTPUT_CHANNELS 2
int usb_audio_channels = 2;                       // 1
int usb_audio_sample_rate = 16000;
int usb_audio_channel_layout = AV_CH_LAYOUT_MONO; // AV_CH_LAYOUT_STEREO
static int usb_frame_size = 128;

// rtsp 回调，连接rtsp成功后，接收server端发来的音视频数据
// type = 0 视频 1 音频
typedef void (*rtsp_cb)(void *buf, int len, int time, int type, long param);

int is_start = 0;
int usb_is_start = 0;
static void sigterm_handler(int sig) {
	printf("get sig:%d\n", sig);
	is_start = 0;
	usb_is_start = 0;
}


static AVFormatContext *ifmt_ctx1 = NULL;
static AVFormatContext *ifmt_ctx2 = NULL;
static AVFormatContext *ifmt_ctx3 = NULL;
static AVCodecContext *dec_ctx1;
static AVCodecContext *dec_ctx2;
static AVCodecContext *dec_ctx3;

AVAudioFifo *fifo1 = NULL;
AVAudioFifo *fifo2 = NULL;
AVAudioFifo *fifo3 = NULL;
pthread_mutex_t counter_mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t counter_mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t counter_mutex3 = PTHREAD_MUTEX_INITIALIZER;

rtsp_cb cb;
static int videoindex;
static int audioindex;
static int audioindex1;
static int audioindex2;
pthread_t recv_id1;
pthread_t recv_id2;


//static AVCodecContext *dec_ctx;
static AVFormatContext *ifmt_ctx = NULL;
pthread_t usb_recv_id;

pthread_t recv_filter_id;

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


//#define DEBUG 

AVFormatContext *output_format_context = NULL;
AVCodecContext *output_codec_context = NULL;


AVFilterGraph *graph;
AVFilterContext *src0, *src1, *src2, *sink;

static int init_fifo(AVAudioFifo **fifo)
{
    /* Create the FIFO buffer based on the specified output sample format. */
    //if (!(*fifo = av_audio_fifo_alloc(output_codec_context->sample_fmt,
     //                                 output_codec_context->channels, 1))) {
    // if (!(*fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_S16,
    if (!(*fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP,
                                      2, 1))) {
        fprintf(stderr, "Could not allocate FIFO\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}
static int init_fifo3(AVAudioFifo **fifo)
{
    if (!(*fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_S16,
                                      /*2*/usb_audio_channels, 1))) {
        fprintf(stderr, "Could not allocate FIFO\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}



static char *const get_error_text(const int error)
{
    static char error_buffer[255];
    av_strerror(error, error_buffer, sizeof(error_buffer));
    return error_buffer;
}

static int init_filter_graph(AVFilterGraph **graph, AVFilterContext **src0,
								AVFilterContext **src1,
								AVFilterContext **src2,
								AVFilterContext **sink)
{
    AVFilterGraph *filter_graph;
    AVFilterContext *abuffer2_ctx;
    const AVFilter        *abuffer2;
    AVFilterContext *abuffer1_ctx;
    const AVFilter        *abuffer1;
	AVFilterContext *abuffer0_ctx;
    const AVFilter        *abuffer0;
    AVFilterContext *mix_ctx;
    const AVFilter        *mix_filter;
    AVFilterContext *abuffersink_ctx;
    const AVFilter        *abuffersink;

    AVFilterContext *volume2_ctx;
    const AVFilter        *volume2;
    AVFilterContext *volume1_ctx;
    const AVFilter        *volume1;
    AVFilterContext *volume0_ctx;
    const AVFilter        *volume0;
    AVFilterContext *volume_sink_ctx;
    const AVFilter        *volume_sink;

	
	char args[512];

	// filter 输入参数
	int sample_rate0 = 48000;
	int sample_fmt0 = AV_SAMPLE_FMT_S16 ;
	uint64_t channel_layout0 = AV_CH_LAYOUT_STEREO;

	int sample_rate1 = 48000;
	int sample_fmt1 = AV_SAMPLE_FMT_S16 ;
	uint64_t channel_layout1 = AV_CH_LAYOUT_STEREO;

	int sample_rate2 = 16000;
	int sample_fmt2 = AV_SAMPLE_FMT_S16 ;
	//uint64_t channel_layout2 = AV_CH_LAYOUT_STEREO;
	uint64_t channel_layout2 = usb_audio_channel_layout;
	
    int err;
	
    /* Create a new filtergraph, which will contain all the filters. */
    filter_graph = avfilter_graph_alloc();
    if (!filter_graph) {
        av_log(NULL, AV_LOG_ERROR, "Unable to create filter graph.\n");
        return AVERROR(ENOMEM);
    }
	
	/****** abuffer 0 ********/
    
    /* Create the abuffer filter;
     * it will be used for feeding the data into the graph. */
    abuffer0 = avfilter_get_by_name("abuffer");
    if (!abuffer0) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the abuffer filter.\n");
        return AVERROR_FILTER_NOT_FOUND;
    }
	
	/* buffer audio source: the decoded frames from the decoder will be inserted here. */
#if 0
    snprintf(args, sizeof(args),
			 "sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
             sample_rate0,
             av_get_sample_fmt_name(sample_fmt0), 
			 channel_layout0);
#else
    snprintf(args, sizeof(args),
			 "sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
			dec_ctx1->sample_rate,
			av_get_sample_fmt_name(dec_ctx1->sample_fmt),
			av_get_default_channel_layout(dec_ctx1->channels));
#endif

    av_log(NULL, AV_LOG_INFO, "src0 input format:%s", args);
	
	err = avfilter_graph_create_filter(&abuffer0_ctx, abuffer0, "src0",
                                       args, NULL, filter_graph);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        return err;
    }

    /****** volume src 0 ******* */
    /* Create volume filter. */
    volume0 = avfilter_get_by_name("volume");
    if (!volume0) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the volume filter.\n");
        return AVERROR_FILTER_NOT_FOUND;
    }
    
    snprintf(args, sizeof(args), "volume=0.1");
	
	err = avfilter_graph_create_filter(&volume0_ctx, volume0, "volume0",
                                       args, NULL, filter_graph);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio amix filter\n");
        return err;
    }


    
	/****** abuffer 1 ******* */
	
	/* Create the abuffer filter;
     * it will be used for feeding the data into the graph. */
    abuffer1 = avfilter_get_by_name("abuffer");
    if (!abuffer1) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the abuffer filter.\n");
        return AVERROR_FILTER_NOT_FOUND;
    }
	
	/* buffer audio source: the decoded frames from the decoder will be inserted here. */
#if 0
    snprintf(args, sizeof(args),
			 "sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
             sample_rate1,
             av_get_sample_fmt_name(sample_fmt1),
			 channel_layout1);
#else
    snprintf(args, sizeof(args),
			 "sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
			dec_ctx2->sample_rate,
			av_get_sample_fmt_name(dec_ctx2->sample_fmt),
			av_get_default_channel_layout(dec_ctx2->channels));
#endif

    av_log(NULL, AV_LOG_INFO, "src1 input format:%s", args);
	
	err = avfilter_graph_create_filter(&abuffer1_ctx, abuffer1, "src1",
                                       args, NULL, filter_graph);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        return err;
    }

    /****** volume src 1 ******* */
    /* Create volume filter. */
    volume1 = avfilter_get_by_name("volume");
    if (!volume0) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the volume filter.\n");
        return AVERROR_FILTER_NOT_FOUND;
    }
    
    snprintf(args, sizeof(args), "volume=1");
	
	err = avfilter_graph_create_filter(&volume1_ctx, volume1, "volume1",
                                       args, NULL, filter_graph);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio volume filter\n");
        return err;
    }


	

	/****** abuffer 2 ******* */
	
	/* Create the abuffer filter;
     * it will be used for feeding the data into the graph. */
    abuffer2 = avfilter_get_by_name("abuffer");
    if (!abuffer2) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the abuffer filter.\n");
        return AVERROR_FILTER_NOT_FOUND;
    }
	
	/* buffer audio source: the decoded frames from the decoder will be inserted here. */
#if 1
    snprintf(args, sizeof(args),
			 "sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
             sample_rate2,
             av_get_sample_fmt_name(sample_fmt2),
			 channel_layout2);
#else
	// sample_rate=48000:sample_fmt=s16:channel_layout=0x3
    snprintf(args, sizeof(args),
			 "sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
			dec_ctx2->sample_rate,
			av_get_sample_fmt_name(dec_ctx2->sample_fmt),
			av_get_default_channel_layout(dec_ctx2->channels));
#endif

    av_log(NULL, AV_LOG_INFO, "src2 input format:%s", args);

	err = avfilter_graph_create_filter(&abuffer2_ctx, abuffer2, "src2",
                                       args, NULL, filter_graph);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        return err;
    }

    /****** volume src 2 ******* */
    /* Create volume filter. */
    volume2 = avfilter_get_by_name("volume");
    if (!volume0) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the volume filter.\n");
        return AVERROR_FILTER_NOT_FOUND;
    }
    
    snprintf(args, sizeof(args), "volume=1");
	
	err = avfilter_graph_create_filter(&volume2_ctx, volume2, "volume2",
                                       args, NULL, filter_graph);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio volume filter\n");
        return err;
    }
	
    /****** amix ******* */
    /* Create mix filter. */
    mix_filter = avfilter_get_by_name("amix");
    if (!mix_filter) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the mix filter.\n");
        return AVERROR_FILTER_NOT_FOUND;
    }
    
    snprintf(args, sizeof(args), "inputs=3");
	
	err = avfilter_graph_create_filter(&mix_ctx, mix_filter, "amix",
                                       args, NULL, filter_graph);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio amix filter\n");
        return err;
    }

	/****** volume sink ******* */
    /* Create volume filter. */
    volume_sink = avfilter_get_by_name("volume");
    if (!volume0) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the volume filter.\n");
        return AVERROR_FILTER_NOT_FOUND;
    }
    
    snprintf(args, sizeof(args), "volume=1");
	
	err = avfilter_graph_create_filter(&volume_sink_ctx, volume_sink, "volume_sink",
                                       args, NULL, filter_graph);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio volume filter\n");
        return err;
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

	err = avfilter_link(abuffer0_ctx, 0, volume0_ctx, 0);
	if(err>=0)
		err = avfilter_link(volume0_ctx, 0, mix_ctx, 0);
	if (err >= 0)
		err = avfilter_link(abuffer1_ctx, 0, volume1_ctx, 0);
	if(err>=0)
        err = avfilter_link(volume1_ctx, 0, mix_ctx, 1);
	if (err >= 0)
		err = avfilter_link(abuffer2_ctx, 0, volume2_ctx, 0);
	if(err>=0)
        err = avfilter_link(volume2_ctx, 0, mix_ctx, 2);
	if (err >= 0)
        err = avfilter_link(mix_ctx, 0, volume_sink_ctx, 0);
	if (err >= 0)
        err = avfilter_link(volume_sink_ctx, 0, abuffersink_ctx, 0);
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
    
    char* dump =avfilter_graph_dump(filter_graph, NULL);
    av_log(NULL, AV_LOG_ERROR, "Graph :\n%s\n", dump);
	av_free(dump);
	
    *graph = filter_graph;
    *src0   = abuffer0_ctx;
	*src1   = abuffer1_ctx;
	*src2   = abuffer2_ctx;
    *sink  = abuffersink_ctx;
    
    return 0;
}

/**
 * Open an output file and the required encoder.
 * Also set some basic encoder parameters.
 * Some of these parameters are based on the input file's parameters.
 */
static int open_output_file(const char *filename,
                            AVFormatContext **output_format_context,
                            AVCodecContext **output_codec_context)
{
    AVIOContext *output_io_context = NULL;
    AVStream *stream               = NULL;
    AVCodec *output_codec          = NULL;
    int error;
	
    /** Open the output file to write to it. */
    if ((error = avio_open(&output_io_context, filename,
                           AVIO_FLAG_WRITE)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s' (error '%s')\n",
               filename, get_error_text(error));
        return error;
    }
	
    /** Create a new format context for the output container format. */
    if (!(*output_format_context = avformat_alloc_context())) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate output format context\n");
        return AVERROR(ENOMEM);
    }
	
    /** Associate the output file (pointer) with the container format context. */
    (*output_format_context)->pb = output_io_context;
	
    /** Guess the desired container format based on the file extension. */
    if (!((*output_format_context)->oformat = av_guess_format(NULL, filename,
                                                              NULL))) {
        av_log(NULL, AV_LOG_ERROR, "Could not find output file format\n");
        goto cleanup;
    }
	
    av_strlcpy((*output_format_context)->filename, filename,
               sizeof((*output_format_context)->filename));
	
    /** Find the encoder to be used by its name. */
    if (!(output_codec = avcodec_find_encoder(AV_CODEC_ID_PCM_S16LE))) {
        av_log(NULL, AV_LOG_ERROR, "Could not find an PCM encoder.\n");
        goto cleanup;
    }
	
    /** Create a new audio stream in the output file container. */
    if (!(stream = avformat_new_stream(*output_format_context, output_codec))) {
        av_log(NULL, AV_LOG_ERROR, "Could not create new stream\n");
        error = AVERROR(ENOMEM);
        goto cleanup;
    }
	
    /** Save the encoder context for easiert access later. */
    *output_codec_context = stream->codec;
	
    /**
     * Set the basic encoder parameters.
     */
    (*output_codec_context)->channels       = OUTPUT_CHANNELS;
    (*output_codec_context)->channel_layout = av_get_default_channel_layout(OUTPUT_CHANNELS);
    (*output_codec_context)->sample_rate    = 48000;//input_codec_context->sample_rate;
    (*output_codec_context)->sample_fmt     = AV_SAMPLE_FMT_S16;
    //(*output_codec_context)->bit_rate       = input_codec_context->bit_rate;
    
    av_log(NULL, AV_LOG_INFO, "output bitrate %ld\n", (*output_codec_context)->bit_rate);
	
    /**
     * Some container formats (like MP4) require global headers to be present
     * Mark the encoder so that it behaves accordingly.
     */
    //if ((*output_format_context)->oformat->flags & AVFMT_GLOBALHEADER)
        //(*output_codec_context)->flags |= CODEC_FLAG_GLOBAL_HEADER;
	
    /** Open the encoder for the audio stream to use it later. */
    if ((error = avcodec_open2(*output_codec_context, output_codec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open output codec (error '%s')\n",
               get_error_text(error));
        goto cleanup;
    }
	
    return 0;
	
cleanup:
    avio_close((*output_format_context)->pb);
    avformat_free_context(*output_format_context);
    *output_format_context = NULL;
    return error < 0 ? error : AVERROR_EXIT;
}

/** Write the header of the output file container. */
static int write_output_file_header(AVFormatContext *output_format_context)
{
    int error;
    if ((error = avformat_write_header(output_format_context, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not write output file header (error '%s')\n",
               get_error_text(error));
        return error;
    }
    return 0;
}

/** Write the trailer of the output file container. */
static int write_output_file_trailer(AVFormatContext *output_format_context)
{
    int error;
    if ((error = av_write_trailer(output_format_context)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not write output file trailer (error '%s')\n",
               get_error_text(error));
        return error;
    }
    return 0;
}




void *recv_proc1(void *param)
{
	int ret;
	AVPacket pkt;
    AVFrame *frame = av_frame_alloc();

	while(is_start) {
		ret = av_read_frame(ifmt_ctx1, &pkt);
		if(ret < 0) {
			printf("error to read frame\n");
			break;
		}

		// 等待filter 初始化完成
		if(src0 == NULL && src1 == NULL){
			av_packet_unref(&pkt);
			usleep(10*1000);
			continue;
		}

		// 解码音频
		if(pkt.stream_index == audioindex1) {
			ret = avcodec_send_packet(dec_ctx1, &pkt);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
				break;
			}

			while (ret >= 0) {
				ret = avcodec_receive_frame(dec_ctx1, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					break;
				} else if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
					goto end;
				}

				if (ret >= 0) {
#if 0
					/* push the audio data from decoded frame into the filtergraph */
					ret = av_buffersrc_add_frame_flags(src0, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
					//ret = av_buffersrc_write_frame(src0, frame);
					if (ret < 0) {
						av_log(NULL, AV_LOG_ERROR, "Error while feeding the audio filtergraph\n");
						break;
					}
#else
					int frame_size = frame->nb_samples;
				pthread_mutex_lock(&counter_mutex1);
					if(av_audio_fifo_write(fifo1, (void **)frame->data, frame_size) < frame_size)
					{
						printf("fifo size:%d\n", av_audio_fifo_size(fifo1));
						fprintf(stderr, "cound not write data to fifo\n");
						exit(1);
					}
				pthread_mutex_unlock(&counter_mutex1);


#endif
					av_frame_unref(frame);
				}
			}	
		}

		//释放内存
		av_free_packet(&pkt);
		//av_packet_unref(&pkt);
	}

end:
	// ？这句话有用吗？没有这句话，会检测到有内存泄漏
	av_free_packet(&pkt);
	av_frame_free(&frame);

}

void *recv_proc2(void *param)
{
	int ret;
	AVPacket pkt;
    AVFrame *frame = av_frame_alloc();

	while(is_start) {
		ret = av_read_frame(ifmt_ctx2, &pkt);
		if(ret < 0) {
			printf("error to read frame\n");
			break;
		}

		// 等待filter 初始化完成
		if(src0 == NULL && src1 == NULL){
			av_packet_unref(&pkt);
			usleep(10*1000);
			continue;
		}



		// 解码音频
		if(pkt.stream_index == audioindex2) {
			ret = avcodec_send_packet(dec_ctx2, &pkt);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
				break;
			}

			while (ret >= 0) {
				ret = avcodec_receive_frame(dec_ctx2, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					break;
				} else if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
					goto end;
				}

				if (ret >= 0) {
#if 0
					/* push the audio data from decoded frame into the filtergraph */
					//ret = av_buffersrc_add_frame_flags(src1, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
					//printf("===============================> src1 write frame\n" );
					ret = av_buffersrc_write_frame(src1, frame);
					if (ret < 0) {
						av_log(NULL, AV_LOG_ERROR, "Error while feeding the audio filtergraph\n");
						break;
					}
#else
					int frame_size = frame->nb_samples;
					//printf("frame_size:%d\n", frame_size);
					assert(fifo2);
					assert(frame_size > 0);
				pthread_mutex_lock(&counter_mutex2);
					if(av_audio_fifo_write(fifo2, (void **)frame->data, frame_size) < frame_size)
					{
						printf("fifo size:%d\n", av_audio_fifo_size(fifo2));
						fprintf(stderr, "cound not write data to fifo\n");
						exit(1);
					}
				pthread_mutex_unlock(&counter_mutex2);

#endif
					av_frame_unref(frame);
				}
			}	
		}



		//释放内存
		av_free_packet(&pkt);
		//av_packet_unref(&pkt);
	}

end:
	// ？这句话有用吗？没有这句话，会检测到有内存泄漏
	av_free_packet(&pkt);
	av_frame_free(&frame);

}


/** Initialize one data packet for reading or writing. */
static void init_packet(AVPacket *packet)
{
    av_init_packet(packet);
    /** Set the packet data and size so that it is recognized as being empty. */
    packet->data = NULL;
    packet->size = 0;
}

/** Encode one frame worth of audio to the output file. */
static int encode_audio_frame(AVFrame *frame,
                              AVFormatContext *output_format_context,
                              AVCodecContext *output_codec_context,
                              int *data_present)
{
    /** Packet used for temporary storage. */
    AVPacket output_packet;
    int error;
    init_packet(&output_packet);
	
    /**
     * Encode the audio frame and store it in the temporary packet.
     * The output audio stream encoder is used to do this.
     */
    if ((error = avcodec_encode_audio2(output_codec_context, &output_packet,
                                       frame, data_present)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not encode frame (error '%s')\n",
               get_error_text(error));
        av_free_packet(&output_packet);
        return error;
    }
	
    /** Write one audio frame from the temporary packet to the output file. */
    if (*data_present) {
        if ((error = av_write_frame(output_format_context, &output_packet)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not write frame (error '%s')\n",
                   get_error_text(error));
            av_free_packet(&output_packet);
            return error;
        }
		
        av_free_packet(&output_packet);
    }
	
    return 0;
}

// 接收filter 后的数据
void *recv_filter_proc(void *param)
{
	int data_present;
    AVFilterContext* buffer_contexts[3];
    int nb_inputs = 3;

	int ret;

	while(is_start) {
		// 等待filter 初始化完成
		if(sink == NULL || src0 == NULL || src1== NULL || src2 == NULL){
			printf("===========================> debug !!%d\n", __LINE__);
			usleep(10*1000);
			continue;
		}

		buffer_contexts[0] = src0;
		buffer_contexts[1] = src1;
		buffer_contexts[2] = src2;

		AVFrame *filt_frame = av_frame_alloc();



		ret = av_buffersink_get_frame(sink, filt_frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
#if 0
			for(int i = 0 ; i < nb_inputs; i++){
				if(av_buffersrc_get_nb_failed_requests(buffer_contexts[i]) > 0){
					av_log(NULL, AV_LOG_INFO, "Need to read input %d\n", i);
				}
			}
#endif

			//printf("===========================> debug !!%d\n", __LINE__);
			
			if(filt_frame)
			av_frame_free(&filt_frame);
			// 等待下次输入数据
			usleep(1*1000);
			continue;

		}
		if (ret < 0)
		{
			if(filt_frame)
			av_frame_free(&filt_frame);
			printf("===========================> error !!!get frame sin\n");
			goto end;
		}

#ifdef DEBUG
#if 0
		av_log(NULL, AV_LOG_INFO, "remove %d samples from sink\n",
				filt_frame->nb_samples); 
#endif
#endif

		//memset(filt_frame->data[0], 0, filt_frame->nb_samples);
		_rtsp_cb(filt_frame->data[0], filt_frame->nb_samples, 0, 1, param);

#if 0
		// filt_frame 是从filter取出来的数据可以保存文件或者播放出来
		///*
		//av_log(NULL, AV_LOG_INFO, "Data read from graph\n");
		ret = encode_audio_frame(filt_frame, output_format_context, output_codec_context, &data_present);
		if (ret < 0)
			goto end;
		//*/
#endif
		av_frame_unref(filt_frame);
		//av_frame_free(&filt_frame);

	}
	av_log(NULL, AV_LOG_INFO, "will exit in :%d", __LINE__);
end:
    if (ret < 0 && ret != AVERROR_EOF) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));
        exit(1);
    }
}

static int _out = 0;
#define TIME_OUT 3000 // ms
static int interruptcb(void *ctx) {
	if(_out) return 0;
	
	return 0;
}

int init_rtsp()
{
	int ret;
	size_t buffer_size, avio_ctx_buffer_size = 4096;
	//const char *in_filename = "rtsp://172.20.2.18/live/main";
	//const char *in_filename = "rtsp://172.20.2.18/h264/main";
	const char *in_filename1 = "rtsp://172.20.2.7/h264/720p";

	const char *in_filename2 = "rtsp://172.20.2.7/h264/1080p";

	cb = _rtsp_cb;

	{
	// 打开保存文件
	FILE* fp = fopen("video_out.h264", "wb");
	if(!fp) {
		printf("ERROR to open video out file\n");
		return -1;
	}
	out_file.video = fp;
	fp = fopen("audio_out.pcm", "wb");
	if(!fp) {
		printf("ERROR to open audio out file\n");
		return -1;
	}
	out_file.audio = fp;
	}

	// rtsp1
	{
		// 设置超时等属性
		AVDictionary* options = NULL;
#if 1
		av_dict_set(&options, "rtsp_transport", "tcp", 0);
		av_dict_set(&options, "stimeout", "1000000", 0); //没有用

		ifmt_ctx1 = avformat_alloc_context();

		// 设置超时回调函数后上面的超时属性才生效, 可以在超时函数中处理其他逻辑
		//AVIOInterruptCB icb = {interruptcb, NULL};
		//ifmt_ctx1->interrupt_callback = icb;
#endif

		// 记录当前时间
		//gettimeofday(&start_time, NULL);

		// 初始化输入
		if ((ret = avformat_open_input(&ifmt_ctx1, in_filename1, 0, &options)) < 0) {
			printf( "Could not open input file.");
			av_dict_free(&options);
			goto end;
		}
		av_dict_free(&options);
		// 连接成功后中断中不检查了
		_out = 1;

		if ((ret = avformat_find_stream_info(ifmt_ctx1, 0)) < 0) {
			printf( "Failed to retrieve input stream information");
			goto end;
		}
		for(int i=0; i<ifmt_ctx1->nb_streams; i++)
			if(ifmt_ctx1->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
				videoindex=i;
				break;
			}

		for(int i=0; i<ifmt_ctx1->nb_streams; i++)
			if(ifmt_ctx1->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
				audioindex=i;
				break;
			}

		printf("dump format:\n");
		// 打印流媒体信息
		av_dump_format(ifmt_ctx1, 0, in_filename1, 0);

		AVCodec *dec;
		/* select the audio stream */
		ret = av_find_best_stream(ifmt_ctx1, AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot find an audio stream in the input file\n");
			return ret;
		}
		audioindex1 = ret;


		dec_ctx1 = avcodec_alloc_context3(dec);
		if (!dec_ctx1)
			return AVERROR(ENOMEM);
		avcodec_parameters_to_context(dec_ctx1, ifmt_ctx1->streams[audioindex1]->codecpar);

		/* init the audio decoder */
		if ((ret = avcodec_open2(dec_ctx1, dec, NULL)) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot open audio decoder\n");
			return ret;
		}
	}

	// rtsp2
	{
		// 设置超时等属性
		AVDictionary* options = NULL;
#if 1
		av_dict_set(&options, "rtsp_transport", "tcp", 0);
		av_dict_set(&options, "stimeout", "1000000", 0); //没有用

		ifmt_ctx2 = avformat_alloc_context();

		// 设置超时回调函数后上面的超时属性才生效, 可以在超时函数中处理其他逻辑
		//AVIOInterruptCB icb = {interruptcb, NULL};
		//ifmt_ctx2->interrupt_callback = icb;
#endif

		// 记录当前时间
		//gettimeofday(&start_time, NULL);

		// 初始化输入
		if ((ret = avformat_open_input(&ifmt_ctx2, in_filename2, 0, &options)) < 0) {
			printf( "Could not open input file.");
			av_dict_free(&options);
			goto end;
		}
		av_dict_free(&options);
		// 连接成功后中断中不检查了
		_out = 1;

		if ((ret = avformat_find_stream_info(ifmt_ctx2, 0)) < 0) {
			printf( "Failed to retrieve input stream information");
			goto end;
		}
		for(int i=0; i<ifmt_ctx2->nb_streams; i++)
			if(ifmt_ctx2->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
				videoindex=i;
				break;
			}

		for(int i=0; i<ifmt_ctx2->nb_streams; i++)
			if(ifmt_ctx2->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
				audioindex=i;
				break;
			}

		printf("will dump format:\n");
		// 打印流媒体信息
		av_dump_format(ifmt_ctx2, 0, in_filename2, 0);

		AVCodec *dec;
		/* select the audio stream */
		ret = av_find_best_stream(ifmt_ctx2, AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot find an audio stream in the input file\n");
			return ret;
		}
		audioindex2 = ret;


		dec_ctx2 = avcodec_alloc_context3(dec);
		if (!dec_ctx2)
			return AVERROR(ENOMEM);
		avcodec_parameters_to_context(dec_ctx2, ifmt_ctx2->streams[audioindex2]->codecpar);

		/* init the audio decoder */
		if ((ret = avcodec_open2(dec_ctx2, dec, NULL)) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot open audio decoder\n");
			return ret;
		}
	}

    if (init_fifo(&fifo1))
	{
		fprintf(stderr, "ERROR TO create fifo1\n");
		exit(1);
	}

    if (init_fifo(&fifo2))
	{
		fprintf(stderr, "ERROR TO create fifo2\n");
		exit(1);
	}

    if (init_fifo3(&fifo3))
	{
		fprintf(stderr, "ERROR TO create fifo2\n");
		exit(1);
	}


	printf("====================================================>\n");
	printf("====================================================>\n");

	printf("%d  %s dec1 layout:%ld\n", 
			dec_ctx1->sample_rate,
			av_get_sample_fmt_name(dec_ctx1->sample_fmt),
			av_get_default_channel_layout(dec_ctx1->channels));
	printf("%d  %s dec2 layout:%ld\n", 
			dec_ctx2->sample_rate,
			av_get_sample_fmt_name(dec_ctx2->sample_fmt),
			av_get_default_channel_layout(dec_ctx2->channels));


	printf("====================================================>\n");
	printf("====================================================>\n");

	is_start = 1;

	// 创建线程接收数据
	recv_id1 = 0;
	ret = pthread_create(&recv_id1, NULL, recv_proc1, NULL);
	if(ret != 0){
		printf("error to create thread:errno = %d\n", errno);
		goto end;
	}

	printf("====================================================>%d\n",__LINE__);
	recv_id2 = 0;
	ret = pthread_create(&recv_id2, NULL, recv_proc2, NULL);
	if(ret != 0){
		printf("error to create thread:errno = %d\n", errno);
		goto end;
	}

#if 0
	printf("====================================================>%d\n",__LINE__);
	recv_filter_id = 0;
	ret = pthread_create(&recv_filter_id, NULL, recv_filter_proc, (void*)&out_file);
	if(ret != 0){
		printf("error to create thread:errno = %d\n", errno);
		goto end;
	}
	printf("====================================================>%d\n",__LINE__);
#endif

	return 0;

end:
	avformat_free_context(ifmt_ctx1);
	avformat_close_input(&ifmt_ctx1);
	ifmt_ctx1 = NULL;

	avformat_free_context(ifmt_ctx2);
	avformat_close_input(&ifmt_ctx2);
	ifmt_ctx2 = NULL;
	return -1;
}

int deinit_rtsp()
{
	is_start = 0;
	printf("==========================>%d\n", __LINE__);
	if(recv_id1!=0) {
		pthread_join(recv_id1, NULL);
		recv_id1 = 0;
	}

	printf("==========================>%d\n", __LINE__);

	if(recv_id2!=0) {
		pthread_join(recv_id2, NULL);
		recv_id2 = 0;
	}

	printf("==========================>%d\n", __LINE__);
	if(recv_filter_id!=0) {
		pthread_join(recv_filter_id, NULL);
		recv_filter_id = 0;
	}
	printf("==========================>%d\n", __LINE__);

	if(ifmt_ctx1) {
		avformat_close_input(&ifmt_ctx1);
		avformat_free_context(ifmt_ctx1);
		ifmt_ctx1 = NULL;
	}
	printf("==========================>%d\n", __LINE__);

	if(ifmt_ctx2) {
		avformat_close_input(&ifmt_ctx2);
		avformat_free_context(ifmt_ctx2);
		ifmt_ctx1 = NULL;
	}
	printf("==========================>%d\n", __LINE__);


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

void *usb_recv_proc(void *param)
{
	int ret;
	AVPacket pkt;

	int frame_size =0;

#if 1
    AVFrame *frame = av_frame_alloc();
#endif

	while(usb_is_start) {
		// usb camera接收数据帧大小固定的
		ret = av_read_frame(ifmt_ctx, &pkt);
		if(ret < 0) {
			printf("error to read frame\n");
			break;
		}

		//av_log(NULL, AV_LOG_DEBUG, "packet pkt size:%d\n", pkt.size);
		//printf("packet pkt size:%d\n", pkt.size);
		//cb(pkt.data, pkt.size, 0, 1, param);
		if(src2!= NULL && src1!= NULL && src0 != NULL)
		{
#if 0
			assert(dec_ctx3);
			ret = avcodec_send_packet(dec_ctx3, &pkt);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
				break;
			}

			while (ret >= 0) {
				ret = avcodec_receive_frame(dec_ctx3, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					break;
				} else if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
					//goto end;
					exit(1);
				}

				if (ret >= 0) {

					int frame_size = frame->nb_samples;
					usb_frame_size = frame->nb_samples;
					//printf("pkt_size=%d\n", frame->pkt_size);
					//printf("af->sample_size=%d\n", fifo3->sample_size);
				
					/*
					printf("AV_CH_LAYOUT_STEREO = %d %d\n", 
							av_get_default_channel_layout(AV_CH_LAYOUT_STEREO) ,frame->channel_layout);
					assert(av_get_default_channel_layout(AV_CH_LAYOUT_STEREO) == frame->channel_layout);
					*/
					//assert(AV_SAMPLE_FMT_S16 ==  frame->format);
					//assert(16000 == frame->sample_rate );
					//assert(0 == memcmp(frame->data[0], pkt.data, 512)); data[0] //存储下所有数据
					//assert(0 == memcmp(frame->data[1], pkt.data+256, 256));
					assert(fifo3);
					assert(frame_size > 0);
				pthread_mutex_lock(&counter_mutex3);
					if(av_audio_fifo_write(fifo3, (void **)frame->data, frame_size) < frame_size)
					{
						printf("fifo size:%d\n", av_audio_fifo_size(fifo3));
						fprintf(stderr, "cound not write data to fifo\n");
						exit(1);
					}
				pthread_mutex_unlock(&counter_mutex3);
					av_frame_unref(frame);
				}
			}
#endif



#if 1
			// 可以直接从package包中获取数据，然后生成AVFrame 包，将数据放入AVFrame包中
			int pkt_size = pkt.size;
			AVFrame *frame;
			frame = av_frame_alloc();

			frame->nb_samples     = pkt_size/2/usb_audio_channels;
			//frame->channel_layout = av_get_default_channel_layout(AV_CH_LAYOUT_STEREO);
			frame->channel_layout = AV_CH_LAYOUT_STEREO;
			frame->format         = AV_SAMPLE_FMT_S16;
			frame->sample_rate    = 16000;

			if ((ret = av_frame_get_buffer(frame, 0)) < 0) {
				fprintf(stderr, "Could not allocate output frame samples (error '%s')\n",
						av_err2str(ret));
				av_frame_free(&frame);
				return ret;
			}
			av_frame_make_writable (frame);
			av_frame_set_pkt_size(frame, pkt_size);


			int frame_size = frame->nb_samples;
			//printf("usb ====> frame_size:%d\n", frame_size);
			//printf("usb ====> nb_samples:%d\n", frame->nb_samples);
			//printf("usb ====>  pkt_size:%d %d\n", frame->pkt_size, pkt_size);
			memcpy(frame->data[0], pkt.data, pkt_size);

			assert(fifo3);
			assert(pkt_size> 0);
			assert(frame->nb_samples> 0);

			pthread_mutex_lock(&counter_mutex3);
			//if(av_audio_fifo_write(fifo3, (void **)frame->data, pkt_size) < pkt_size)
			if(av_audio_fifo_write(fifo3, (void **)&pkt.data, frame_size) < frame_size)
			{
				printf("fifo size:%d\n", av_audio_fifo_size(fifo3));
				fprintf(stderr, "cound not write data to fifo\n");
				exit(1);
			}
			pthread_mutex_unlock(&counter_mutex3);


			//av_frame_unref(frame);
			av_frame_free(&frame);
#endif
		}

		//av_free_packet(&pkt);
		av_packet_unref(&pkt);
	}

	av_frame_free(&frame);

//end:
	av_log(NULL, AV_LOG_INFO, "recv proc exit\n");

	// ？这句话有用吗？没有这句话，会检测到有内存泄漏
	//av_free_packet(&pkt);
	av_packet_unref(&pkt);

}


int init_audio_capture(const char *in_filename, const char * channel)
{
	AVInputFormat *ifmt;

	int ret;

	char errors[1024] = {0};

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
	AVDictionary * options = NULL;
	//av_dict_set(&options, "channels", "2", 0);
	//av_dict_set(&options, "channels", "2", 0);
	av_dict_set(&options, "channels", channel, 0);
	//av_dict_set(&options, "sample_rate", "32000", 0);
	// 可能需要编译 alsa 支持的ffmpeg dev 库
	av_dict_set(&options, "sample_rate", "16000", 0);

	if(avformat_open_input(&ifmt_ctx, in_filename, ifmt, &options)!=0){  
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
	av_dump_format(ifmt_ctx, 0, in_filename, 0);

    /** Open the decoder for the audio stream to use it later. */
    if ((ret = avcodec_open2(ifmt_ctx->streams[0]->codec,
                               dec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open input codec (error '%s')\n",
               get_error_text(ret));
        avformat_close_input(&ifmt_ctx);
		exit(1);
    }
	
    //input_codec_context = ifmt_ctx->streams[0]->codec;
	dec_ctx3 = ifmt_ctx->streams[0]->codec;





	// 创建线程接收数据
	usb_is_start = 1;
	usb_recv_id = 0;
	ret = pthread_create(&usb_recv_id, NULL, usb_recv_proc, (void *)&out_file);
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
int deinit_audio_capture()
{
	usb_is_start = 0;
	if(usb_recv_id!=0) {
		pthread_join(usb_recv_id, NULL);
		usb_recv_id = 0;
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

/** Initialize one audio frame for reading from the input file */
static int init_input_frame(AVFrame **frame)
{
    if (!(*frame = av_frame_alloc())) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate input frame\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}



void *mix_proc(void *param)
{
	int ret = 0;

	int data_present = 0;
	int finished = 0;

	int nb_inputs = 3;

	/*
	AVFormatContext* input_format_contexts[2];
	AVCodecContext* input_codec_contexts[2];
	input_format_contexts[0] = ifmt_ctx1;//input_format_context_0;
	input_format_contexts[1] = ifmt_ctx2;//input_format_context_1;
	input_format_contexts[1] = ifmt_ctx2;//input_format_context_1;
	input_codec_contexts[0] = dec_ctx1;//input_codec_context_0;
	input_codec_contexts[1] = dec_ctx2;//input_codec_context_1;
	input_codec_contexts[1] = dec_ctx2;//input_codec_context_1;
	*/

	AVFilterContext* buffer_contexts[3];
	buffer_contexts[0] = src0;
	buffer_contexts[1] = src1;
	buffer_contexts[2] = src2;
	assert(src0);
	assert(src1);
	assert(src2);

	int input_finished[3];
	input_finished[0] = 0;
	input_finished[1] = 0;
	input_finished[2] = 0;

	int input_to_read[3];
	input_to_read[0] = 1;
	input_to_read[1] = 1;
	input_to_read[2] = 1;

	int total_samples[3];
	total_samples[0] = 0;
	total_samples[1] = 0;
	total_samples[2] = 0;

	int total_out_samples = 0;

	int nb_finished = 0;



	while (nb_finished < nb_inputs) {
		int data_present_in_graph = 0;

		// 读取数据
		for(int i = 0 ; i < nb_inputs ; i++){
			if(input_finished[i] || input_to_read[i] == 0){
				continue;
			}
			input_to_read[i] = 0;

			AVFrame *frame = NULL;

			if(init_input_frame(&frame) > 0){
				printf("=============>error :%d\n", __LINE__);
				goto end;
			}
#if 0
			/** Decode one frame worth of audio samples. */
			if ( (ret = decode_audio_frame(frame, input_format_contexts[i], input_codec_contexts[i], &data_present, &finished))){
				goto end;
			}
#else
			int frame_size = 1024;

			frame->channel_layout = AV_CH_LAYOUT_STEREO;
			if(i== 2)
			{
				frame->format         = AV_SAMPLE_FMT_S16;
				frame->sample_rate    = 16000;
				//frame_size = 128;
				frame_size = usb_frame_size;
				// todo frame_size 可以设置为128 或者动态读取
				//printf("usb_frame_size=%d  av_audio_fifo_size=%d\n", usb_frame_size, av_audio_fifo_size(fifo3));
				
				//frame->channel_layout = AV_CH_LAYOUT_MONO;
				frame->channel_layout = usb_audio_channel_layout;
			}
			else {
				frame->format         = AV_SAMPLE_FMT_FLTP;
				frame->sample_rate    = 48000;
			}
			frame->nb_samples     = frame_size;

			if ((ret = av_frame_get_buffer(frame, 0)) < 0) {
				fprintf(stderr, "Could not allocate output frame samples (error '%s')\n",
						av_err2str(ret));
				av_frame_free(&frame);
				exit(1);
			}

			ret = av_frame_make_writable(frame);
			assert(ret == 0);
			ret = av_frame_is_writable(frame);
			assert(ret != 0);

again:
			if(i == 0) {
				assert(fifo1);
				assert(frame->data[0]);
				pthread_mutex_lock(&counter_mutex1);
#ifdef DEBUG
				fprintf(stderr, "FIFO, file size:%d\n", av_audio_fifo_size(fifo1));
#endif
				if (av_audio_fifo_read(fifo1, (void **)frame->data, frame_size) < frame_size) {
#ifdef DEBUG
					fprintf(stderr, "Could not read data from FIFO, fifo1 file size:%d\n", av_audio_fifo_size(fifo1));
#endif
					pthread_mutex_unlock(&counter_mutex1);
					usleep(20*1000);	
					if(is_start == 0) {
						printf("%d will exit\n", __LINE__);
						av_frame_free(&frame);
						goto end;
					}
					goto again;
							
					//return AVERROR_EXIT;
				}
				pthread_mutex_unlock(&counter_mutex1);
			} else if(i == 1) {
				assert(fifo2);
				assert(frame->data[0]);
				pthread_mutex_lock(&counter_mutex2);
				if (av_audio_fifo_read(fifo2, (void **)frame->data, frame_size) < frame_size) {
				//pthread_mutex_unlock(&counter_mutex2);
#ifdef DEBUG
					fprintf(stderr, "Could not read data from FIFO, fifo2 file size:%d\n", av_audio_fifo_size(fifo2));
#endif
					//av_frame_free(&frame);
					pthread_mutex_unlock(&counter_mutex2);
					if(is_start == 0){
						printf("%d will exit\n", __LINE__);
						av_frame_free(&frame);
						goto end;
					}
					usleep(20*1000);	
					goto again;
					//return AVERROR_EXIT;
				}
				pthread_mutex_unlock(&counter_mutex2);
			} else {
				assert(fifo3);
				assert(frame->data[0]);
				//printf("=============================> usb audio i=%d\n", i);
				pthread_mutex_lock(&counter_mutex3);
				if (av_audio_fifo_read(fifo3, (void **)frame->data, frame_size) < frame_size) {
				//pthread_mutex_unlock(&counter_mutex2);
#ifdef DEBUG
					fprintf(stderr, "Could not read data from FIFO, fifo3 file size:%d\n", av_audio_fifo_size(fifo3));
#endif
					//av_frame_free(&frame);
					pthread_mutex_unlock(&counter_mutex3);
					if(is_start == 0){
						printf("%d will exit\n", __LINE__);
						av_frame_free(&frame);
						goto end;
					}
					usleep(20*1000);	
					goto again;
					//return AVERROR_EXIT;
				}
				pthread_mutex_unlock(&counter_mutex3);

			}

			data_present = 1;

#endif

			/**
			 * If we are at the end of the file and there are no more samples
			 * in the decoder which are delayed, we are actually finished.
			 * This must not be treated as an error.
			 */
			if (finished && !data_present) {
				input_finished[i] = 1;
				nb_finished++;
				ret = 0;
				av_log(NULL, AV_LOG_INFO, "Input n°%d finished. Write NULL frame \n", i);

				ret = av_buffersrc_write_frame(buffer_contexts[i], NULL);
				if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Error writing EOF null frame for input %d\n", i);
					goto end;
				}
			}
			else if (data_present) { /** If there is decoded data, convert and store it */
				/* push the audio data from decoded frame into the filtergraph */
				ret = av_buffersrc_write_frame(buffer_contexts[i], frame);
				if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Error while feeding the audio filtergraph\n");
					goto end;
				}

#ifdef DEBUG
#if 0
				av_log(NULL, AV_LOG_INFO, "add %d samples on input %d (%d Hz, time=%f, ttime=%f)\n",
						frame->nb_samples, i, input_codec_contexts[i]->sample_rate,
						(double)frame->nb_samples / input_codec_contexts[i]->sample_rate,
						(double)(total_samples[i] += frame->nb_samples) / input_codec_contexts[i]->sample_rate);
#endif

#if 1
				if(i == 2)
				av_log(NULL, AV_LOG_INFO, "add %d samples on input %d (%d Hz, time=%f, ttime=%f)\n",
						frame->nb_samples, i, 16000,
						(double)frame->nb_samples / 16000,
						(double)(total_samples[i] += frame->nb_samples) / 16000);
				else
				av_log(NULL, AV_LOG_INFO, "add %d samples on input %d (%d Hz, time=%f, ttime=%f)\n",
						frame->nb_samples, i, 48000,
						(double)frame->nb_samples / 48000,
						(double)(total_samples[i] += frame->nb_samples) / 48000);
#endif

#endif

			}

			av_frame_free(&frame);

			data_present_in_graph = data_present | data_present_in_graph;
		}





		if(data_present_in_graph){
			AVFrame *filt_frame = av_frame_alloc();

			/* pull filtered audio from the filtergraph */
			while (1) {
				ret = av_buffersink_get_frame(sink, filt_frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
					for(int i = 0 ; i < nb_inputs ; i++){
						if(av_buffersrc_get_nb_failed_requests(buffer_contexts[i]) > 0){
							input_to_read[i] = 1;
#ifdef DEBUG
#if 1
							av_log(NULL, AV_LOG_INFO, "Need to read input %d\n", i);
#endif
#endif
						}
					}

					break;
				}
				if (ret < 0){
				printf("=============>error :%d\n", __LINE__);
					goto end;
				}

#ifdef DEBUG
#if 1
				av_log(NULL, AV_LOG_INFO, "remove %d samples from sink (%d Hz, time=%f, ttime=%f)\n",
						filt_frame->nb_samples, output_codec_context->sample_rate,
						(double)filt_frame->nb_samples / output_codec_context->sample_rate,
						(double)(total_out_samples += filt_frame->nb_samples) / output_codec_context->sample_rate);
#endif
#endif

				//av_log(NULL, AV_LOG_INFO, "Data read from graph\n");
				ret = encode_audio_frame(filt_frame, output_format_context, output_codec_context, &data_present);
				if (ret < 0) {
					printf("=============>error :%d\n", __LINE__);
					goto end;
				}

				av_frame_unref(filt_frame);
			}

			av_frame_free(&filt_frame);
		} else {
			av_log(NULL, AV_LOG_INFO, "No data in graph\n");
			for(int i = 0 ; i < nb_inputs ; i++){
				input_to_read[i] = 1;
			}
		}

	}

	return 0;

end:
	//    avcodec_close(input_codec_context);
	//    avformat_close_input(&input_format_context);
	//    av_frame_free(&frame);
	//    av_frame_free(&filt_frame);

	if (ret < 0 && ret != AVERROR_EOF) {
		av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));
		exit(1);
	}

	//exit(0);


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
	av_register_all();
	avfilter_register_all();
	avdevice_register_all();



#if 1
	// arm 
	const char *in_filename = "hw:1";
	const char * channel = "2";

	usb_audio_channels = 2;                       // 1
	usb_audio_sample_rate = 16000;
	usb_audio_channel_layout = AV_CH_LAYOUT_STEREO;
#else
	// x86
	const char *in_filename = "hw:3";
	const char * channel = "1";
	usb_audio_channels = 1;                       // 1
	usb_audio_sample_rate = 16000;
	usb_audio_channel_layout = AV_CH_LAYOUT_MONO; // AV_CH_LAYOUT_STEREO
#endif
	err = init_audio_capture(in_filename, channel);
	assert(err == 0);
	ifmt_ctx3 = ifmt_ctx;	



	printf("================> before init rtsp\n");
	init_rtsp();
	printf("================> after init rtsp\n");


    /* Set up the filtergraph. */
    err = init_filter_graph(&graph, &src0, &src1, &src2, &sink);
	printf("Init err = %d\n", err);

	char* outputFile = "output.wav";
    remove(outputFile);
    
	av_log(NULL, AV_LOG_INFO, "Output file : %s\n", outputFile);

	printf("====================================================>%d\n",__LINE__);
	
	// err = open_output_file(outputFile, input_codec_context_0, &output_format_context, &output_codec_context);
	err = open_output_file(outputFile, &output_format_context, &output_codec_context);
	printf("open output file err : %d\n", err);
	av_dump_format(output_format_context, 0, outputFile, 1);
	
	if(write_output_file_header(output_format_context) < 0){
		av_log(NULL, AV_LOG_ERROR, "Error while writing header outputfile\n");
		exit(1);
	}

while(av_audio_fifo_size(fifo1) <= 0 && av_audio_fifo_size(fifo2) <= 0
		&& av_audio_fifo_size(fifo3) <= 0
		) {
		printf("fifo no data\n");
		usleep(500*1000);
	}
	printf("=======================> all fifo have data:%d %d %d\n",
			av_audio_fifo_size(fifo1), av_audio_fifo_size(fifo2), av_audio_fifo_size(fifo3));



#if 0
	while(is_start){
		usleep(50*1000);
	}
#else

	mix_proc(NULL);
#endif

	printf("====================================================>%d\n",__LINE__);

	deinit_rtsp();
	deinit_audio_capture();
	avfilter_graph_free(&graph);

    if (fifo1)
        av_audio_fifo_free(fifo1);
    if (fifo2)
        av_audio_fifo_free(fifo2);
    if (fifo3)
        av_audio_fifo_free(fifo3);

	return 0;
}
