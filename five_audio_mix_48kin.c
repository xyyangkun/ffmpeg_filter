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
#include "audio_play.h"
#include "audio_card_recv.h"
#include "SDL2/SDL.h"
#include <unistd.h>
#include <assert.h>



#define OUTPUT_CHANNELS 2
//int usb_audio_channels = 2;                       // 1
//int usb_audio_sample_rate = 16000;
//int usb_audio_channel_layout = AV_CH_LAYOUT_MONO; // AV_CH_LAYOUT_STEREO
static int usb_frame_size = 128;
static int usb_frame_size_trans = 128;

const int alsa_out_samples = 1024;

// rtsp 回调，连接rtsp成功后，接收server端发来的音视频数据
// type = 0 视频 1 音频
typedef void (*rtsp_cb)(void *buf, int len, int time, int type, long param);

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

// 音频通路选择
static volatile int audio_sel = 5;
/*
 0 选通输入第1路音频
 1 选通输入第2路音频
 2 选通输入第3路音频
 3 选通输入第4路音频
 4 选通输入第5路音频
 5 选通输出音频
 */

//  输入的5路声音是否存在
//  1 表示有输入源，从输入源获取数据 0表示没有输入源，全部是静音数据
static volatile int audio0_exist=0;
static volatile int audio1_exist=0;
static volatile int audio2_exist=0;
static volatile int audio3_exist=0;
static volatile int audio4_exist=0;

#define AUIDIO_TICK_INTERVAL    21



// 打开声卡
FHANDLE play_hdmi_hd = NULL;
FHANDLE play_35_hd = NULL;

static AVFormatContext *ifmt_ctx1 = NULL;
static AVFormatContext *ifmt_ctx2 = NULL;
static AVFormatContext *ifmt_ctx3 = NULL;
static AVCodecContext *dec_ctx1;
static AVCodecContext *dec_ctx2;
static AVCodecContext *dec_ctx3;

AVAudioFifo *fifo1 = NULL;
AVAudioFifo *fifo2 = NULL;
AVAudioFifo *fifo3 = NULL;
AVAudioFifo *fifo4 = NULL;
AVAudioFifo *fifo5 = NULL;

pthread_mutex_t counter_mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t counter_mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t counter_mutex3 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t counter_mutex4 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t counter_mutex5 = PTHREAD_MUTEX_INITIALIZER;


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
AVFilterContext *src0, *src1, *src2, *src3, *src4, *sink;

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
#if 0
static int init_fifo3(AVAudioFifo **fifo)
{
    if (!(*fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_S16,
                                      /*2*/usb_audio_channels, 1))) {
        fprintf(stderr, "Could not allocate FIFO\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}
#endif

static int init_fifo_test(AVAudioFifo **fifo)
{
    if (!(*fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_S16,
                                      2, 1))) {
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
								AVFilterContext **src3,
								AVFilterContext **src4,
								AVFilterContext **sink)
{
    AVFilterGraph *filter_graph;

    AVFilterContext *abuffer4_ctx;
    const AVFilter        *abuffer4;

    AVFilterContext *abuffer3_ctx;
    const AVFilter        *abuffer3;

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

    AVFilterContext *volume4_ctx;
    const AVFilter        *volume4;

    AVFilterContext *volume3_ctx;
    const AVFilter        *volume3;

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

#if 0
	// 正常读取usb 数据
	// 
	int sample_rate2 = usb_audio_sample_rate;
	int sample_fmt2 = AV_SAMPLE_FMT_S16 ;
	//uint64_t channel_layout2 = AV_CH_LAYOUT_STEREO;
	uint64_t channel_layout2 = usb_audio_channel_layout;
#else
	// 将usb 获取的所有音频数据转换成立体声48k 数据
	int sample_rate2 = 48000;
	int sample_fmt2 = AV_SAMPLE_FMT_S16 ;
	uint64_t channel_layout2 = AV_CH_LAYOUT_STEREO;
	//uint64_t channel_layout2 = usb_audio_channel_layout;
#endif

	int sample_rate3 = 48000;
	int sample_fmt3 = AV_SAMPLE_FMT_S16 ;
	uint64_t channel_layout3 = AV_CH_LAYOUT_STEREO;

	int sample_rate4 = 48000;
	int sample_fmt4 = AV_SAMPLE_FMT_S16 ;
	uint64_t channel_layout4 = AV_CH_LAYOUT_STEREO;
	
    int err;
	
    /* Create a new filtergraph, which will contain all the filters. */
    filter_graph = avfilter_graph_alloc();
    if (!filter_graph) {
        av_log(NULL, AV_LOG_ERROR, "Unable to create filter graph.\n");
		assert(1);
        return AVERROR(ENOMEM);
    }
	
	/****** abuffer 0 ********/
    
    /* Create the abuffer filter;
     * it will be used for feeding the data into the graph. */
    abuffer0 = avfilter_get_by_name("abuffer");
    if (!abuffer0) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the abuffer filter.\n");
		assert(1);
        return AVERROR_FILTER_NOT_FOUND;
    }
	
	/* buffer audio source: the decoded frames from the decoder will be inserted here. */
#if 1
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
		assert(1);
        return err;
    }
	assert(abuffer0_ctx);

    /****** volume src 0 ******* */
    /* Create volume filter. */
    volume0 = avfilter_get_by_name("volume");
    if (!volume0) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the volume filter.\n");
		assert(1);
        return AVERROR_FILTER_NOT_FOUND;
    }
    
    snprintf(args, sizeof(args), "volume=1");
	
	err = avfilter_graph_create_filter(&volume0_ctx, volume0, "volume0",
                                       args, NULL, filter_graph);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio amix filter\n");
		assert(1);
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
#if 1
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
    if (!volume1) {
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
    if (!volume2) {
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



	/****** abuffer 3 ******* */
	/* Create the abuffer filter;
     * it will be used for feeding the data into the graph. */
    abuffer3 = avfilter_get_by_name("abuffer");
    if (!abuffer3) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the abuffer filter.\n");
        return AVERROR_FILTER_NOT_FOUND;
    }
	
	/* buffer audio source: the decoded frames from the decoder will be inserted here. */
#if 1
    snprintf(args, sizeof(args),
			 "sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
             sample_rate3,
             av_get_sample_fmt_name(sample_fmt3),
			 channel_layout3);
#else
	// sample_rate=48000:sample_fmt=s16:channel_layout=0x3
    snprintf(args, sizeof(args),
			 "sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
			dec_ctx2->sample_rate,
			av_get_sample_fmt_name(dec_ctx2->sample_fmt),
			av_get_default_channel_layout(dec_ctx2->channels));
#endif

    av_log(NULL, AV_LOG_INFO, "src3 input format:%s", args);
	err = avfilter_graph_create_filter(&abuffer3_ctx, abuffer3, "src3",
                                       args, NULL, filter_graph);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        return err;
    }

    /****** volume src 3 ******* */
    /* Create volume filter. */
    volume3 = avfilter_get_by_name("volume");
    if (!volume3) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the volume filter.\n");
        return AVERROR_FILTER_NOT_FOUND;
    }
    
    snprintf(args, sizeof(args), "volume=1");
	err = avfilter_graph_create_filter(&volume3_ctx, volume3, "volume3",
                                       args, NULL, filter_graph);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio volume filter\n");
        return err;
    }



	/****** abuffer 4 ******* */
	/* Create the abuffer filter;
     * it will be used for feeding the data into the graph. */
    abuffer4 = avfilter_get_by_name("abuffer");
    if (!abuffer4) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the abuffer filter.\n");
        return AVERROR_FILTER_NOT_FOUND;
    }
	
	/* buffer audio source: the decoded frames from the decoder will be inserted here. */
#if 1
    snprintf(args, sizeof(args),
			 "sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
             sample_rate4,
             av_get_sample_fmt_name(sample_fmt4),
			 channel_layout4);
#else
	// sample_rate=48000:sample_fmt=s16:channel_layout=0x3
    snprintf(args, sizeof(args),
			 "sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
			dec_ctx2->sample_rate,
			av_get_sample_fmt_name(dec_ctx2->sample_fmt),
			av_get_default_channel_layout(dec_ctx2->channels));
#endif

    av_log(NULL, AV_LOG_INFO, "src3 input format:%s", args);
	err = avfilter_graph_create_filter(&abuffer4_ctx, abuffer4, "src4",
                                       args, NULL, filter_graph);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        return err;
    }

    /****** volume src 4 ******* */
    /* Create volume filter. */
    volume4 = avfilter_get_by_name("volume");
    if (!volume4) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the volume filter.\n");
        return AVERROR_FILTER_NOT_FOUND;
    }
    
    snprintf(args, sizeof(args), "volume=1");
	err = avfilter_graph_create_filter(&volume4_ctx, volume4, "volume4",
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
    
    snprintf(args, sizeof(args), "inputs=5");
	
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
    
    //snprintf(args, sizeof(args), "volume=0.2");
    snprintf(args, sizeof(args), "volume=1");
	
	err = avfilter_graph_create_filter(&volume_sink_ctx, volume_sink, "volume6",
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
		err = avfilter_link(abuffer3_ctx, 0, volume3_ctx, 0);
	if(err>=0)
        err = avfilter_link(volume3_ctx, 0, mix_ctx, 3);

	if (err >= 0)
		err = avfilter_link(abuffer4_ctx, 0, volume4_ctx, 0);
	if(err>=0)
        err = avfilter_link(volume4_ctx, 0, mix_ctx, 4);

	if (err >= 0)
        err = avfilter_link(mix_ctx, 0, volume_sink_ctx, 0);
	if (err >= 0)
        err = avfilter_link(volume_sink_ctx, 0, abuffersink_ctx, 0);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error connecting filters\n");
		assert(1);
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
	*src3   = abuffer3_ctx;
	*src4   = abuffer4_ctx;
    *sink  = abuffersink_ctx;
    
    return 0;
}

/**
 * Open an output file and the required encoder.
 * Also set some basic encoder parameters.
 * Some of these parameters are based on the input file's parameters.
 * @param      filename              File to be opened
 * @param      input_codec_context   Codec context of input file
 * @param[out] output_format_context Format context of output file
 * @param[out] output_codec_context  Codec context of output file
 * @return Error code (0 if successful)
 */
// ffmpeg -i audio10.wav  -f alsa default
static int open_output_alsa_file(const char *filename,
                            //AVCodecContext *input_codec_context,
                            AVFormatContext **output_format_context,
                            AVCodecContext **output_codec_context)
{

#if 1
  // find output format for ALSA device
    AVOutputFormat* fmt = av_guess_format("alsa", NULL, NULL);
    if (!fmt) {
        fprintf(stderr, "av_guess_format()\n");
        exit(1);
    }

    // allocate empty format context
    // provides methods for writing output packets
    AVFormatContext* fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx) {
        fprintf(stderr, "avformat_alloc_context()\n");
        exit(1);
    }

    // tell format context to use ALSA as ouput device
    fmt_ctx->oformat = fmt;

    // add stream to format context
    AVStream* stream = avformat_new_stream(fmt_ctx, NULL);
    if (!stream) {
        fprintf(stderr, "avformat_new_stream()\n");
        exit(1);
    }

	const int in_channels = 2;
	const int in_samples = alsa_out_samples;//1024;
	const int sample_rate = 48000;
	const int bitrate = 64000;

    // initialize stream codec context
    // format conetxt uses codec context when writing packets
    AVCodecContext* codec_ctx = stream->codec;
    assert(codec_ctx);
    //codec_ctx->codec_id = AV_CODEC_ID_PCM_F32LE;
    codec_ctx->codec_id = AV_CODEC_ID_PCM_S16LE;
    codec_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    // codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLT;
    codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    codec_ctx->bit_rate = bitrate;
    codec_ctx->sample_rate = sample_rate;
    codec_ctx->channels = in_channels;
    codec_ctx->channel_layout = AV_CH_FRONT_LEFT | AV_CH_FRONT_RIGHT;

	*output_codec_context = codec_ctx;
	*output_format_context = fmt_ctx;
	return 0;
#else
    /* Open the output file to write to it. */
    if ((error = avio_open(&output_io_context, filename,
                           AVIO_FLAG_WRITE)) < 0) {
        fprintf(stderr, "Could not open output file '%s' (error '%s')\n",
                filename, av_err2str(error));
        return error;
    }

    /* Create a new format context for the output container format. */
    if (!(*output_format_context = avformat_alloc_context())) {
        fprintf(stderr, "Could not allocate output format context\n");
		assert(1);
        return AVERROR(ENOMEM);
    }

    /* Associate the output file (pointer) with the container format context. */
    //(*output_format_context)->pb = output_io_context;

    /* Guess the desired container format based on the file extension. */
    if (!((*output_format_context)->oformat = av_guess_format(NULL, filename,
                                                              NULL))) {
        fprintf(stderr, "Could not find output file format\n");
		assert(1);
        goto cleanup;
    }

    if (!((*output_format_context)->url = av_strdup(filename))) {
        fprintf(stderr, "Could not allocate url.\n");
        error = AVERROR(ENOMEM);
        goto cleanup;
    }

    /* Find the encoder to be used by its name. */
    if (!(output_codec = avcodec_find_encoder(AV_CODEC_ID_AAC))) {
        fprintf(stderr, "Could not find an AAC encoder.\n");
        goto cleanup;
    }

    /* Create a new audio stream in the output file container. */
    if (!(stream = avformat_new_stream(*output_format_context, NULL))) {
        fprintf(stderr, "Could not create new stream\n");
        error = AVERROR(ENOMEM);
        goto cleanup;
    }

    avctx = avcodec_alloc_context3(output_codec);
    if (!avctx) {
        fprintf(stderr, "Could not allocate an encoding context\n");
        error = AVERROR(ENOMEM);
        goto cleanup;
    }

    /* Set the basic encoder parameters.
     * The input file's sample rate is used to avoid a sample rate conversion. */
    avctx->channels       = OUTPUT_CHANNELS;
    avctx->channel_layout = av_get_default_channel_layout(OUTPUT_CHANNELS);
    //avctx->sample_rate    = 16000;
    //avctx->sample_fmt     = AV_SAMPLE_FMT_S16;
    avctx->sample_rate    = 48000;////input_codec_context->sample_rate;
    avctx->sample_fmt     = output_codec->sample_fmts[0];

	printf("sample_fmt:%d\n", avctx->sample_fmt);

    /* Allow the use of the experimental AAC encoder. */
    avctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    /* Set the sample rate for the container. */
    stream->time_base.den = avctx->sample_rate;
    stream->time_base.num = 1;

    /* Some container formats (like MP4) require global headers to be present.
     * Mark the encoder so that it behaves accordingly. */
    if ((*output_format_context)->oformat->flags & AVFMT_GLOBALHEADER)
        avctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    /* Open the encoder for the audio stream to use it later. */
    if ((error = avcodec_open2(avctx, output_codec, NULL)) < 0) {
        fprintf(stderr, "Could not open output codec (error '%s')\n",
                av_err2str(error));
        goto cleanup;
    }

    error = avcodec_parameters_from_context(stream->codecpar, avctx);
    if (error < 0) {
        fprintf(stderr, "Could not initialize stream parameters\n");
        goto cleanup;
    }

    /* Save the encoder context for easier access later. */
    *output_codec_context = avctx;

    return 0;

cleanup:
    avcodec_free_context(&avctx);
    avio_closep(&(*output_format_context)->pb);
    avformat_free_context(*output_format_context);
    *output_format_context = NULL;
    return error < 0 ? error : AVERROR_EXIT;
#endif
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

#if 0
void *recv_proc1(void *param)
{
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
					int frame_size = frame->nb_samples;

#if 1
					/* compute destination number of samples */
					dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx1, src_rate) +
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

					//printf("frame_size:%d dst_nb_samples:%d\n", frame_size, dst_nb_samples);
					//  先转换为48000 s16格式
					/* convert to destination format */
					ret = swr_convert(swr_ctx1, dst_data, dst_nb_samples, (const uint8_t **)frame->data, frame_size);
					if (ret < 0) {
						fprintf(stderr, "Error while converting\n");
						goto end;
					}

					assert(dst_data != NULL);
					pthread_mutex_lock(&counter_mutex1);
					//int size = frame_size*2*2;
					int size = frame_size;
					if(av_audio_fifo_write(fifo1, (void **)dst_data, size) < size)
					{
						printf("fifo size:%d\n", av_audio_fifo_size(fifo1));
						fprintf(stderr, "cound not write data to fifo\n");
						exit(1);
					}
					pthread_mutex_unlock(&counter_mutex1);

#else
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

	if (dst_data)
		av_freep(&dst_data[0]);
	av_freep(&dst_data);

}

void *recv_proc2(void *param)
{
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
					int frame_size = frame->nb_samples;
					//printf("frame_size:%d\n", frame_size);
					assert(fifo2);
					assert(frame_size > 0);

#if 1
					/* compute destination number of samples */
					dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx2, src_rate) +
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

					//printf("frame_size:%d dst_nb_samples:%d\n", frame_size, dst_nb_samples);
					//  先转换为48000 s16格式
					/* convert to destination format */
					ret = swr_convert(swr_ctx2, dst_data, dst_nb_samples, (const uint8_t **)frame->data, frame_size);
					if (ret < 0) {
						fprintf(stderr, "Error while converting\n");
						goto end;
					}

					assert(dst_data != NULL);
					pthread_mutex_lock(&counter_mutex2);
					//int size = frame_size*2*2;
					int size = frame_size;
					if(av_audio_fifo_write(fifo2, (void **)dst_data, size) < size)
					{
						printf("fifo size:%d\n", av_audio_fifo_size(fifo2));
						fprintf(stderr, "cound not write data to fifo\n");
						exit(1);
					}
					pthread_mutex_unlock(&counter_mutex2);

#else

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

	if (dst_data)
		av_freep(&dst_data[0]);
	av_freep(&dst_data);
}
#endif


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

// 音频静音数据
static char audio_data[1024*4]={0};

// 如果某路音频源不存在，就想音频中防止静音数据
// 没21ms放置一次
Uint32 audio_not_exist_cb()
{
	//int size = 1024*4;
	int size = 1024;
	void *data[1];
	data[0] = (void*)audio_data;
	if(audio0_exist == 0  && av_audio_fifo_size(fifo1) <= 1024*8)
	{
		printf("======================xxxxxxxxxxxxxxxxxxxxxxxxxx %d\n", __LINE__);
		pthread_mutex_lock(&counter_mutex1);
		//int size = frame_size;
		if(av_audio_fifo_write(fifo1, (void **)data, size) < size)
		{
			printf("fifo size:%d\n", av_audio_fifo_size(fifo1));
			fprintf(stderr, "cound not write data to fifo\n");
			exit(1);
		}
		pthread_mutex_unlock(&counter_mutex1);


	}

	if(audio1_exist == 0  && av_audio_fifo_size(fifo2) <= 1024*8)
	{
		printf("======================xxxxxxxxxxxxxxxxxxxxxxxxxx %d\n", __LINE__);
		pthread_mutex_lock(&counter_mutex2);
		//int size = frame_size;
		if(av_audio_fifo_write(fifo2, (void **)data, size) < size)
		{
			printf("fifo size:%d\n", av_audio_fifo_size(fifo2));
			fprintf(stderr, "cound not write data to fifo\n");
			exit(1);
		}
		pthread_mutex_unlock(&counter_mutex2);
	
	}

	if(audio2_exist == 0  && av_audio_fifo_size(fifo3) <= 1024*8)
	{
		printf("======================xxxxxxxxxxxxxxxxxxxxxxxxxx %d\n", __LINE__);
		// usb的还需要更改写入值
		pthread_mutex_lock(&counter_mutex3);
		//int size = frame_size;
		if(av_audio_fifo_write(fifo3, (void **)data, size) < size)
		{
			printf("fifo size:%d\n", av_audio_fifo_size(fifo3));
			fprintf(stderr, "cound not write data to fifo\n");
			exit(1);
		}
		pthread_mutex_unlock(&counter_mutex3);

	}

	if(audio3_exist == 0  && av_audio_fifo_size(fifo4) <= 1024*8)
	{
		//printf("======================xxxxxxxxxxxxxxxxxxxxxxxxxx %d\n", __LINE__);
		pthread_mutex_lock(&counter_mutex4);
		//int size = frame_size;
		if(av_audio_fifo_write(fifo4, (void **)data, size) < size)
		{
			printf("fifo size:%d\n", av_audio_fifo_size(fifo4));
			fprintf(stderr, "cound not write data to fifo\n");
			exit(1);
		}
		pthread_mutex_unlock(&counter_mutex4);
	}

	if(audio4_exist == 0 && av_audio_fifo_size(fifo5) <= 1024*8)
	{
		//printf("======================xxxxxxxxxxxxxxxxxxxxxxxxxx %d\n", __LINE__);
		
		pthread_mutex_lock(&counter_mutex5);
		//int size = frame_size;
		if(av_audio_fifo_write(fifo5, (void **)data, size) < size)
		{
			printf("fifo size:%d\n", av_audio_fifo_size(fifo5));
			fprintf(stderr, "cound not write data to fifo\n");
			exit(1);
		}
		pthread_mutex_unlock(&counter_mutex5);
	
	}
	return AUIDIO_TICK_INTERVAL;

}

int init_5fifo()
{
	// 初始化3个输入fifo
    if (init_fifo_test(&fifo1))
    //if (init_fifo(&fifo1))
	{
		fprintf(stderr, "ERROR TO create fifo1\n");
		exit(1);
	}

    if (init_fifo_test(&fifo2))
    //if (init_fifo(&fifo2))
	{
		fprintf(stderr, "ERROR TO create fifo2\n");
		exit(1);
	}

    if (init_fifo_test(&fifo3))
    //if (init_fifo3(&fifo3))
	{
		fprintf(stderr, "ERROR TO create fifo3\n");
		exit(1);
	}

    if (init_fifo_test(&fifo4))
	{
		fprintf(stderr, "ERROR TO create fifo4\n");
		exit(1);
	}

    if (init_fifo_test(&fifo5))
	{
		fprintf(stderr, "ERROR TO create fifo4\n");
		exit(1);
	}

	return 0;
}

int deinit_5fifo()
{
    if (fifo1)
        av_audio_fifo_free(fifo1);
    if (fifo2)
        av_audio_fifo_free(fifo2);
    if (fifo3)
        av_audio_fifo_free(fifo3);
    if (fifo4)
        av_audio_fifo_free(fifo4);
    if (fifo5)
        av_audio_fifo_free(fifo5);

	return 0;
}

#if 0
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
	printf("==========================>%d\n", __LINE__);


	return 0;
}

void *usb_recv_proc(void *param)
{
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


	int frame_samples = usb_frame_size;// 1024;
	int src_rate = usb_audio_sample_rate;
	assert(usb_audio_sample_rate != 0);
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
			_frame = av_frame_alloc();

			_frame->nb_samples     = pkt_size/2/usb_audio_channels;
			//_frame->channel_layout = av_get_default_channel_layout(AV_CH_LAYOUT_STEREO);
			_frame->channel_layout = AV_CH_LAYOUT_STEREO;
			_frame->format         = AV_SAMPLE_FMT_S16;
			_frame->sample_rate    = 16000;

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

			assert(fifo3);
			assert(pkt_size> 0);
			assert(_frame->nb_samples> 0);
#if 1
			// 转换成48k双通道后入栈

			/* compute destination number of samples */
			dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx3, src_rate) +
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
			ret = swr_convert(swr_ctx3, dst_data, dst_nb_samples, (const uint8_t **)&pkt.data, frame_size);
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
			usb_frame_size_trans = size;
		

			pthread_mutex_lock(&counter_mutex3);
			//if(av_audio_fifo_write(fifo3, (void **)dst_data, frame_size) < frame_size)
			if(av_audio_fifo_write(fifo3, (void **)dst_data, size) < size)
			{
				printf("fifo size:%d\n", av_audio_fifo_size(fifo3));
				fprintf(stderr, "cound not write data to fifo\n");
				exit(1);
			}
			pthread_mutex_unlock(&counter_mutex3);


#else
			// 16k usb数据直接入fifo
			pthread_mutex_lock(&counter_mutex3);
			//if(av_audio_fifo_write(fifo3, (void **)frame->data, pkt_size) < pkt_size)
			if(av_audio_fifo_write(fifo3, (void **)&pkt.data, frame_size) < frame_size)
			{
				printf("fifo size:%d\n", av_audio_fifo_size(fifo3));
				fprintf(stderr, "cound not write data to fifo\n");
				exit(1);
			}
			pthread_mutex_unlock(&counter_mutex3);
#endif


			//av_frame_unref(frame);
			av_frame_free(&_frame);
#endif
		}

		//av_free_packet(&pkt);
		av_packet_unref(&pkt);
	}


end:
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

int init_audio_capture(const char *in_filename, const char * channel)
{
	AVInputFormat *ifmt;

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
	av_dict_set(&options, "sample_rate", "16000", 0);

	//av_dict_set(&options, "thread_queue_size", "1024", 0);

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

#endif

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

	int nb_inputs = 5;

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

	AVFilterContext* buffer_contexts[5];
	buffer_contexts[0] = src0;
	buffer_contexts[1] = src1;
	buffer_contexts[2] = src2;
	buffer_contexts[3] = src3;
	buffer_contexts[4] = src4;
	assert(src0);
	assert(src1);
	assert(src2);
	assert(src3);
	assert(src4);

	int input_finished[5];
	input_finished[0] = 0;
	input_finished[1] = 0;
	input_finished[2] = 0;
	input_finished[3] = 0;
	input_finished[4] = 0;

	int input_to_read[5];
	input_to_read[0] = 1;
	input_to_read[1] = 1;
	input_to_read[2] = 1;
	input_to_read[3] = 1;
	input_to_read[4] = 1;

	int total_samples[5];
	total_samples[0] = 0;
	total_samples[1] = 0;
	total_samples[2] = 0;
	total_samples[3] = 0;
	total_samples[4] = 0;

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
#if 0
				// 直接处理usb数据
				frame->format         = AV_SAMPLE_FMT_S16;
				frame->sample_rate    = 16000;
				//frame_size = 128;
				frame_size = usb_frame_size;
				// todo frame_size 可以设置为128 或者动态读取
				//printf("usb_frame_size=%d  av_audio_fifo_size=%d\n", usb_frame_size, av_audio_fifo_size(fifo3));
				
				//frame->channel_layout = AV_CH_LAYOUT_MONO;
				frame->channel_layout = usb_audio_channel_layout;
#else
				// 转换成48k 立体声之后的数据

				frame->format         = AV_SAMPLE_FMT_S16;
				frame->sample_rate    = 48000;
				frame_size = usb_frame_size_trans;//432;
				frame->channel_layout = AV_CH_LAYOUT_STEREO;
#endif
			}
			else if(i== 0)
			{
				frame->format         = AV_SAMPLE_FMT_S16;
				frame->sample_rate    = 48000;
				// todo frame_size 可以设置为128 或者动态读取
				//printf("frame_size=%d  av_audio_fifo_size=%d\n", usb_frame_size, av_audio_fifo_size(fifo1));
				
				//frame->channel_layout = AV_CH_LAYOUT_MONO;
				//frame->channel_layout = usb_audio_channel_layout;
			}
			else if(i== 1)
			{
				frame->format         = AV_SAMPLE_FMT_S16;
				frame->sample_rate    = 48000;
				// todo frame_size 可以设置为128 或者动态读取
				//printf("frame_size=%d  av_audio_fifo_size=%d\n", usb_frame_size, av_audio_fifo_size(fifo2));
				
				//frame->channel_layout = AV_CH_LAYOUT_MONO;
				//frame->channel_layout = usb_audio_channel_layout;
			}
			else {
				// aac 解码后的格式为fltp  直接采集的格式为s16
				//frame->format         = AV_SAMPLE_FMT_FLTP;
				frame->format         = AV_SAMPLE_FMT_S16;
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
				fprintf(stderr, "FIFO, file size:%d, frame_size\n", av_audio_fifo_size(fifo1), frame_size);
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
			} else if(i == 2)  {
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
			else if(i == 3)  {
				assert(fifo4);
				assert(frame->data[0]);
				//printf("=============================> usb audio i=%d\n", i);
				pthread_mutex_lock(&counter_mutex4);
				if (av_audio_fifo_read(fifo4, (void **)frame->data, frame_size) < frame_size) {
				//pthread_mutex_unlock(&counter_mutex2);
#ifdef DEBUG
					fprintf(stderr, "Could not read data from FIFO, fifo4 file size:%d\n", av_audio_fifo_size(fifo4));
#endif
					//av_frame_free(&frame);
					pthread_mutex_unlock(&counter_mutex4);
					if(is_start == 0){
						printf("%d will exit\n", __LINE__);
						av_frame_free(&frame);
						goto end;
					}
					usleep(20*1000);	
					goto again;
					//return AVERROR_EXIT;
				}
				pthread_mutex_unlock(&counter_mutex4);

			}
			else if(i == 4)  {
				assert(fifo5);
				assert(frame->data[0]);
				//printf("=============================> usb audio i=%d\n", i);
				pthread_mutex_lock(&counter_mutex5);
				if (av_audio_fifo_read(fifo5, (void **)frame->data, frame_size) < frame_size) {
				//pthread_mutex_unlock(&counter_mutex2);
#ifdef DEBUG
					fprintf(stderr, "Could not read data from FIFO, fifo5 file size:%d\n", av_audio_fifo_size(fifo5));
#endif
					//av_frame_free(&frame);
					pthread_mutex_unlock(&counter_mutex5);
					if(is_start == 0){
						printf("%d will exit\n", __LINE__);
						av_frame_free(&frame);
						goto end;
					}
					usleep(20*1000);	
					goto again;
					//return AVERROR_EXIT;
				}
				pthread_mutex_unlock(&counter_mutex5);

			}else {
				printf("WRONG index\n");
				exit(1);
			}



			// 根据选通选择hdmi输出
			if(audio_sel == i)
			{
				audio_play_out(play_hdmi_hd, frame->data, frame_size);
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

#if 0
				if(i == 2)
				av_log(NULL, AV_LOG_INFO, "add %d samples on input %d (%d Hz, time=%f, ttime=%f)\n",
						frame->nb_samples, i, 48000,
						(double)frame->nb_samples / 48000,
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
#if 0
				av_log(NULL, AV_LOG_INFO, "remove %d samples from sink (%d Hz, time=%f, ttime=%f)\n",
						filt_frame->nb_samples, output_codec_context->sample_rate,
						(double)filt_frame->nb_samples / output_codec_context->sample_rate,
						(double)(total_out_samples += filt_frame->nb_samples) / output_codec_context->sample_rate);
#endif
#endif

#if 0
				//av_log(NULL, AV_LOG_INFO, "Data read from graph\n");
				ret = encode_audio_frame(filt_frame, output_format_context, output_codec_context, &data_present);
				if (ret < 0) {
					printf("=============>error :%d\n", __LINE__);
					goto end;
				}
#else
				// 写alsa程序
				/*
				printf("size:filt_frame:%d\n", filt_frame->pkt_size);
				printf("size:nb_samples:%d\n", filt_frame->nb_samples);
				assert(48000 == filt_frame->sample_rate);
				assert(AV_SAMPLE_FMT_S16 == filt_frame->format);
				assert(AV_CH_LAYOUT_STEREO == filt_frame->channel_layout);
				//*/
#if 0
				// 老的方式输出
				AVPacket packet;
				av_new_packet(&packet, alsa_out_samples*2*2);
				//assert(packet.size == 1024*2*2);

				memcpy(packet.data, filt_frame->data[0], packet.size);
				if (av_write_frame(output_format_context, &packet) < 0) {
					fprintf(stderr, "av_write_frame()\n");
					exit(1);
				}

				av_free_packet(&packet);
#else
				// 通过声卡直接输出
				audio_play_out(play_35_hd, filt_frame->data, alsa_out_samples);
				if(audio_sel == 5)
				{
					audio_play_out(play_hdmi_hd, filt_frame->data, alsa_out_samples);
				}

#endif

#endif

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

// 设置视频丢失通道
int set_lose(int lose_chn,int value)
{
	assert(value == 0 || value == 1);
	if(lose_chn==0) {
		audio0_exist = value;
	}else if(lose_chn==1){
		audio1_exist = value;
	}else if(lose_chn==2){
		audio2_exist = value;
	}else if(lose_chn==3){
		audio3_exist = value;
	}else if(lose_chn==4){
		audio4_exist = value;
	}else
	{

	}

}

// 设置选通通道 
int set_sel(int sel)
{
	audio_sel = sel;
}



/**
 * @brief 设置音量
 * @param[in] index  3 设置输出音量  0:输入1   1:输入2     2:输入3
 * @param[in] volume 输入音量字符串
 * @ return 0 success
 */
int set_mix_volume(unsigned char index, const char *volume) {
	// const char *volume = "0.1";
	// const char *volume = "5";
	// av_opt_set(_volume->priv, "volume", AV_STRINGIFY("volume=0.1"), 0);
	
	int ret;
	
	if(index == 0) {
		// 通过avfilter_graph_send_command 实现
		const char *target = "volume0";
		const char *cmd = "volume";
		const char *arg = volume;
		char *res=NULL;
		int res_len=0;
		int flags=0;
		ret = avfilter_graph_send_command(graph, target, cmd, arg, res, res_len, flags);
		if(ret < 0) {
			printf("graph send comman error!!\n");
		}
	}
	else if(index == 1) {
		// 通过avfilter_graph_send_command 实现
		const char *target = "volume1";
		const char *cmd = "volume";
		const char *arg = volume;
		char *res=NULL;
		int res_len=0;
		int flags=0;
		ret = avfilter_graph_send_command(graph, target, cmd, arg, res, res_len, flags);
		if(ret < 0) {
			printf("graph send comman error!!\n");
		}
	}
	else if(index == 2) {
		// 通过avfilter_graph_send_command 实现
		const char *target = "volume2";
		const char *cmd = "volume";
		const char *arg = volume;
		char *res=NULL;
		int res_len=0;
		int flags=0;
		ret = avfilter_graph_send_command(graph, target, cmd, arg, res, res_len, flags);
		if(ret < 0) {
			printf("graph send comman error!!\n");
		}
	}
	else if(index == 3) {
		// 通过avfilter_graph_send_command 实现
		const char *target = "volume_sink";
		const char *cmd = "volume";
		const char *arg = volume;
		char *res=NULL;
		int res_len=0;
		int flags=0;
		ret = avfilter_graph_send_command(graph, target, cmd, arg, res, res_len, flags);
		if(ret < 0) {
			printf("graph send comman error!!\n");
		}
	}
	else {
		printf("get param error!!!\n");
		return -1;
	}

	return 0;
}

// rtsp 接收1
void audio_recv1(void *buf, unsigned int size, long handle)
{
	//printf("audio_recv1 size:%d\n", size);

	if(audio0_exist == 1)
	{
		pthread_mutex_lock(&counter_mutex1);
		//int size = frame_size;
		if(av_audio_fifo_write(fifo1, (void **)buf, size) < size)
		{
			printf("fifo size:%d\n", av_audio_fifo_size(fifo1));
			fprintf(stderr, "cound not write data to fifo\n");
			exit(1);
		}
		pthread_mutex_unlock(&counter_mutex1);
	}

}

// rtsp 接收2
void audio_recv2(void *buf, unsigned int size, long handle)
{
	//printf("audio_recv2 size:%d\n", size);
	if(audio1_exist == 1)
	{
		pthread_mutex_lock(&counter_mutex2);
		//int size = frame_size;
		if(av_audio_fifo_write(fifo2, (void **)buf, size) < size)
		{
			printf("fifo size:%d\n", av_audio_fifo_size(fifo2));
			fprintf(stderr, "cound not write data to fifo\n");
			exit(1);
		}
		pthread_mutex_unlock(&counter_mutex2);
	}
}

// 判断是否有数据源，有数据源将数据放入fifo
// 没有数据源，不放入

void usb_audio_card_recv(void *buf, unsigned int size, long handle)
{
	// printf("usb audio_card_recv size:%d\n", size);
#if 0
	FHANDLE hd = (FHANDLE)handle;
	audio_play_out(hd, buf, size);
#endif
	if(audio2_exist == 1)
	{

		// usb的还需要更改写入值
		usb_frame_size_trans = size;

		pthread_mutex_lock(&counter_mutex3);
		//int size = frame_size;
		if(av_audio_fifo_write(fifo3, (void **)buf, size) < size)
		{
			printf("fifo size:%d\n", av_audio_fifo_size(fifo3));
			fprintf(stderr, "cound not write data to fifo\n");
			exit(1);
		}
		pthread_mutex_unlock(&counter_mutex3);

	}


}

#ifndef TEST
int main_proc(int argc, const char * argv[]);
// 其他程序引用
void* multi_audio_mix_proc(void *arg) {
	main_proc(0,0);
}
int main_proc(int argc, const char * argv[]){
#else//TEST
// 本程序测试使用
int main(int argc, const char * argv[]){
#endif//TEST

	int ret;

	signal(SIGINT, sigterm_handler);

    
    int err;
	

	ffmpeg_init();

	// 动态获取usb 配置 
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


	//初始化fifo
	init_5fifo();

#if 0
	// 删除代码
	err = init_audio_capture(dev_name, channel);
	assert(err == 0);
	ifmt_ctx3 = ifmt_ctx;	



	printf("================> before init rtsp\n");
	init_rtsp();
	printf("================> after init rtsp\n");
#endif


	// 初始化音频输入
	char *rtsp_url1 = "rtsp://172.20.2.7/h264/720p";
	char *rtsp_url2 = "rtsp://172.20.2.7/h264/1080p";

	FHANDLE recv_hdmi_hd1 = NULL;
	FHANDLE recv_hdmi_hd2 = NULL;

	FHANDLE usb_audio_hd = NULL;
	ret = audio_recv_create(&recv_hdmi_hd1, rtsp_url1, audio_recv1,  (long)fifo1); 
	if(ret != 0)
	{
		printf("ERROR to create audio recv1\n");
		exit(1);
	}
	ret = audio_recv_create(&recv_hdmi_hd2, rtsp_url2, audio_recv2,  (long)fifo2); 
	if(ret != 0)
	{
		printf("ERROR to create audio recv2\n");
		exit(1);
	}
	ret = audio_card_recv_create(&usb_audio_hd, dev_name, &info, usb_audio_card_recv, (long)fifo3);
	if(ret != 0)
	{
		printf("ERROR to create audio recv3\n");
		exit(1);
	}

	is_start = 1;

	// 将前三路设置为1为真实数据
	audio0_exist = 1;
	audio1_exist = 1;
	audio2_exist = 1;

	// 打开声卡
	int hdmi_type = 0;
	ret = audio_play_create(&play_hdmi_hd, hdmi_type);
	if(ret != 0)
	{
		printf("ERROR to create audio play\n");
		exit(1);
	}
	printf("==========================>%d %p\n", __LINE__, play_hdmi_hd);

	int _35_type = 1;
	ret = audio_play_create(&play_35_hd, _35_type);
	if(ret != 0)
	{
		printf("ERROR to create  35 audio play\n");
		exit(1);
	}
	printf("==========================>%d %p\n", __LINE__, play_35_hd);



    /* Set up the filtergraph. */
    err = init_filter_graph(&graph, &src0, &src1, &src2, &src3, &src4, &sink);
	printf("Init err = %d\n", err);

	assert(src0!=NULL);
	assert(src1!=NULL);
	assert(src2!=NULL);
	assert(src3!=NULL);
	assert(src4!=NULL);

#if 0
	//char* outputFile = "output.wav";
    //remove(outputFile);
	char* outputFile = "alsa";
    
	av_log(NULL, AV_LOG_INFO, "Output file : %s\n", outputFile);

	printf("====================================================>%d\n",__LINE__);

	// 删除代码
	// err = open_output_file(outputFile, input_codec_context_0, &output_format_context, &output_codec_context);
	err = open_output_alsa_file(outputFile, &output_format_context, &output_codec_context);
	printf("open output file err : %d\n", err);
	assert(output_format_context);
	av_dump_format(output_format_context, 0, outputFile, 1);
	
	if(write_output_file_header(output_format_context) < 0){
		av_log(NULL, AV_LOG_ERROR, "Error while writing header outputfile\n");
		exit(1);
	}
#endif

	//  添加sdl定时器
	SDL_TimerID timer = SDL_AddTimer(AUIDIO_TICK_INTERVAL, audio_not_exist_cb, NULL);//创立定时器timer 

while(av_audio_fifo_size(fifo1) <= 0 && av_audio_fifo_size(fifo2) <= 0
		&& av_audio_fifo_size(fifo3) <= 0
		&& av_audio_fifo_size(fifo4) <= 0
		&& av_audio_fifo_size(fifo5) <= 0
		) {
		printf("fifo no data\n");
		usleep(500*1000);
	}

	printf("=======================> all fifo have data:%d %d %d %d %d\n",
			av_audio_fifo_size(fifo1), av_audio_fifo_size(fifo2), av_audio_fifo_size(fifo3),
			av_audio_fifo_size(fifo4), av_audio_fifo_size(fifo5));



#if 0
	while(is_start){
		usleep(50*1000);
	}
#else

	mix_proc(NULL);
#endif

	printf("====================================================>%d\n",__LINE__);

#if 0
	// 删除代码
	deinit_rtsp();

	printf("==========================>%d\n", __LINE__);
	deinit_audio_capture();
#endif

	ret = audio_recv_remove(&recv_hdmi_hd1);
	if(ret!=0)
	{
		printf("error in audio recv remove\n");
		exit(-1);
	}

	ret = audio_recv_remove(&recv_hdmi_hd2);
	if(ret!=0)
	{
		printf("error in audio recv remove\n");
		exit(-1);
	}

	ret = audio_card_recv_remove(&usb_audio_hd);
	if(ret!=0)
	{
		printf("error in audio card recv remove\n");
		exit(-1);
	}

	printf("==========================>%d\n", __LINE__);
	avfilter_graph_free(&graph);

	printf("==========================>%d\n", __LINE__);

	// 删除sdl定时器
	SDL_RemoveTimer(timer);

	deinit_5fifo();
	printf("==========================>%d\n", __LINE__);

	audio_play_remove(&play_hdmi_hd);
	audio_play_remove(&play_35_hd);
	printf("==========================>%d\n", __LINE__);

	return 0;
}
