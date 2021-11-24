/******************************************************************************
 *
 *       Filename:  usb_audio.c
 *
 *    Description:  usb audio capture save to pcm
 *    pcm 文件播放方法：
 *    ffplay -f s16le -ac 1 -ar 16000 audio_out.pcm
 *
 *        Version:  1.0
 *        Created:  2021年11月23日 16时25分18秒
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
//#include <libavfilter/avfiltergraph.h>

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
#include <sys/time.h>
#include <assert.h>

#define INPUT_SAMPLERATE     48000
#define INPUT_FORMAT         AV_SAMPLE_FMT_S16
#define INPUT_CHANNEL_LAYOUT AV_CH_LAYOUT_STEREO
/** The output bit rate in kbit/s */
#define OUTPUT_BIT_RATE 48000
/** The number of output channels */
//#define OUTPUT_CHANNELS 2
#define OUTPUT_CHANNELS 1
/** The audio sample output format */
#define OUTPUT_SAMPLE_FORMAT AV_SAMPLE_FMT_S16



// rtsp 回调，连接rtsp成功后，接收server端发来的音视频数据
// type = 0 视频 1 音频
typedef void (*rtsp_cb)(void *buf, int len, int time, int type, long param);

static AVFormatContext *output_format_context = NULL;
static AVCodecContext *output_codec_context = NULL;

int is_start = 0;
static void sigterm_handler(int sig) {
	//printf("=========>get sig:%d %d\n", sig, SIGUSR1);

	if(sig == SIGUSR1/*10*/) {
	
		return;
	}

	is_start = 0;
}

static AVCodecContext *dec_ctx;
static AVFormatContext *ifmt_ctx = NULL;
static int videoindex;
static int audioindex;
pthread_t recv_id;
rtsp_cb cb;

struct s_out_file
{
	FILE* video;
	FILE* audio;
};

struct s_out_file out_file;


static char *const get_error_text(const int error)
{
    static char error_buffer[255];
    av_strerror(error, error_buffer, sizeof(error_buffer));
    return error_buffer;
}

static int open_pcm_file(const char *filename,
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
    //(*output_codec_context)->sample_rate    = input_codec_context->sample_rate;
    (*output_codec_context)->sample_rate    = 16000;
    (*output_codec_context)->sample_fmt     = AV_SAMPLE_FMT_S16;
    //(*output_codec_context)->bit_rate       = input_codec_context->bit_rate;
    
    av_log(NULL, AV_LOG_INFO, "output bitrate %d\n", (*output_codec_context)->bit_rate);
	
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

void *recv_proc(void *param)
{
	int ret;
	AVPacket pkt;

	while(is_start) {
		ret = av_read_frame(ifmt_ctx, &pkt);
		if(ret < 0) {
			printf("error to read frame\n");
			break;
		}

#if 0
		if(pkt.stream_index == audioindex){
			// 因为声卡出来的直接是pcm数据，所以直接写文件
			if ((ret = av_write_frame(output_format_context, &pkt)) < 0) {
				av_log(NULL, AV_LOG_ERROR, "Could not write frame (error '%s')\n",
						get_error_text(ret));
				// av_free_packet(&output_packet);
				av_packet_unref(&pkt);
				goto end;
			}
		}
#else
		//av_log(NULL, AV_LOG_DEBUG, "pkt size:%d\n", pkt.size);
		cb(pkt.data, pkt.size, 0, 1, param);
#endif
		//av_free_packet(&pkt);
		av_packet_unref(&pkt);
	}


//end:
	av_log(NULL, AV_LOG_INFO, "recv proc exit\n");

	// ？这句话有用吗？没有这句话，会检测到有内存泄漏
	//av_free_packet(&pkt);
	av_packet_unref(&pkt);

}

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


int init_audio_capture(const char *in_filename, const char * channel)
{
	AVInputFormat *ifmt;

	int ret;
	{
		cb = _rtsp_cb;

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


	char errors[1024] = {0};

	ifmt = av_find_input_format("alsa");
	if(ifmt == NULL) {
		perror("error to found alsa");
		return -1;
	}
	//assert(ifmt);

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

	dec_ctx = avcodec_alloc_context3(dec);
	if (!dec_ctx)
		return AVERROR(ENOMEM);
	avcodec_parameters_to_context(dec_ctx, ifmt_ctx->streams[audioindex]->codecpar);

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
int deinit_audio_capture()
{
	is_start = 0;
	if(recv_id!=0) {
		pthread_join(recv_id, NULL);
		recv_id = 0;
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

int main(int argc, char **argv)
{
	int ret;
	if(argc != 3) {
		printf("usage:%d  audio_card_name channel     \n"
				"audio_card_name hw:1  hw:3\n"
				"channel:1  or 2\n");
		return -1;
	}
	

	int err;

	signal(SIGINT, sigterm_handler);
	signal(SIGUSR1, sigterm_handler);

    av_log_set_level(AV_LOG_TRACE);

	av_register_all();
	avfilter_register_all();
	avdevice_register_all();

	{
	unsigned version = avcodec_version();
	printf("avcode version:%d\n", version);
	}

	{
	unsigned version = avdevice_version();
	printf("avdevice version:%d\n", version);
	}

	char* outputFile = "output.wav";
    remove(outputFile);

	
	ret = init_audio_capture(argv[1], argv[2]);
	if(ret != 0){
		printf("error init audio\n");
		return ret;
	}

	while(is_start){
		usleep(50*1000);
	}
   

	deinit_audio_capture();

	return 0;
}
