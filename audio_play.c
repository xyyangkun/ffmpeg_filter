/******************************************************************************
 *
 *       Filename:  audio_play.c
 *
 *    Description:  audio play
 *
 *        Version:  1.0
 *        Created:  2021年12月14日 17时48分54秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
/*!
* \file audio_play.h
* \brief 通过声卡播放生意
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

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>

#include "sound_card_get.h"

#include "audio_play.h"


typedef struct s_audio_play
{
	char name[512]; // 声卡名字

	AVFormatContext *output_format_context; //  声卡输出相关
	AVCodecContext *output_codec_context  ;


}t_audio_play;

static int alsa_out_samples = 1024;
static char *const get_error_text(const int error)
{
    static char error_buffer[255];
    av_strerror(error, error_buffer, sizeof(error_buffer));
    return error_buffer;
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
static int open_output_alsa_file(const char *filename,
                            //AVCodecContext *input_codec_context,
                            AVFormatContext **output_format_context,
                            AVCodecContext **output_codec_context)
{

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

}
#endif

int audio_play_create(FHANDLE *hd,const int  type)
{
	int ret;
	t_audio_play *p_audio_play = (t_audio_play *)malloc(sizeof(t_audio_play));
	if(!p_audio_play) 
	{
		printf("ERROR go malloc t_audio_play memory:%d, ==>%s\n", errno, strerror(errno));
		exit(1);
	}
	memset(p_audio_play, 0, sizeof(t_audio_play));



#if 1
	// find output format for ALSA device
    AVOutputFormat* fmt = av_guess_format("alsa", NULL, NULL);
    if (!fmt) {
        fprintf(stderr, "error av_guess_format()\n");
        exit(1);
    }

    // allocate empty format context
    // provides methods for writing output packets
    AVFormatContext* fmt_ctx = avformat_alloc_context();
    //p_audio_play->output_format_context = avformat_alloc_context();
    if (! fmt_ctx) {
        fprintf(stderr, "error in avformat_alloc_context()\n");
        exit(1);
    }

    // tell format context to use ALSA as ouput device
    fmt_ctx->oformat = fmt;
	static char *hdmi_str = "hw:0";
	static char *_35_str = "hw:1";

	// url 是内部变量，内部释放调用av_free
	// 本来应该通过avformat_write_header 的options方法传入，
	// 但是没有找到，直接这样分配吧
	//
    //fmt_ctx->url = "hw:1"; // cat /proc/asound/cards   
	// 这样在栈上分配，本函数退出，是这个地址不存在了，也可能会double free
	// 使用静态变量同样会double free
	fmt_ctx->url = (char *)av_malloc(100);
	//fmt_ctx->url = p_audio_play->name;
#if 1
	if(type == 0) {
		// hdmi
		//fmt_ctx->url = hdmi_str;
		strcpy(fmt_ctx->url, hdmi_str);
	}
	else {
		// 3.5
		strcpy(fmt_ctx->url, _35_str);
	}
#endif

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

	//*output_codec_context = codec_ctx;
	//*output_format_context = fmt_ctx;




	printf("==========================>%d\n", __LINE__);

	p_audio_play->output_format_context = fmt_ctx;
	p_audio_play->output_codec_context =  codec_ctx;
#else
	char* outputFile = "alsa";

	AVCodecContext *output_codec_context = NULL;

	int err = open_output_alsa_file(outputFile, &p_audio_play->output_format_context, &output_codec_context);
	printf("open output file err : %d\n", err);

#endif
	av_dump_format(p_audio_play->output_format_context, 0, p_audio_play->name, 1);




	// 固定格式
	if(write_output_file_header(p_audio_play->output_format_context) < 0){
		av_log(NULL, AV_LOG_ERROR, "Error while writing header outputfile\n");
		exit(1);
	}


	*hd = p_audio_play;

	printf("==========================>%d %p\n", __LINE__, *hd);

	return 0;
end:
	assert(1);
    //avformat_free_context(fmt_ctx);
    avformat_free_context(p_audio_play->output_format_context);

	return ret;
}



int audio_play_remove(FHANDLE *hd)
{

	printf("==========================>%d %p\n", __LINE__, *hd);
	t_audio_play *p_audio_play = (t_audio_play *)(*hd);
	if(*hd == NULL) {
		printf("error to audio play remove !!\n");
		exit(-1);
	}

	printf("==========================>%d %p\n", __LINE__, *hd);

    write_output_file_trailer(p_audio_play->output_format_context);

    avformat_free_context(p_audio_play->output_format_context);
	p_audio_play->output_format_context = NULL;

	free(p_audio_play);
	p_audio_play = NULL;
	*hd = NULL;

	return 0;
}

int audio_play_out(FHANDLE hd, void *buf, unsigned int buf_size)
{
	t_audio_play *p_audio_play = (t_audio_play *)hd;
	if(hd == NULL) {
		printf("error to audio play remove !!\n");
		exit(-1);
	}

#if 0
	AVPacket packet;
	av_new_packet(&packet, alsa_out_samples*2*2);
	//assert(buf_size == packet.size);

	//printf("===========>%d buf_size:%d  pkt_size:%d\n", __LINE__, buf_size, packet.size);
	//memcpy(packet.data, filt_frame->data[0], packet.size);
	//memcpy(packet.data, buf, packet.size/2);
	void **data = (void **)buf;
	memcpy(packet.data, data[0], packet.size);

	if (av_write_frame(p_audio_play->output_format_context, &packet) < 0) {
		fprintf(stderr, "av_write_frame()\n");
		exit(1);
	}

	//printf("==========================>%d %p\n", __LINE__, hd);
	av_free_packet(&packet);
#else

	AVPacket packet;
	av_new_packet(&packet, buf_size*4);
	//assert(buf_size == packet.size);

	void **data = (void **)buf;
	memcpy(packet.data, data[0], packet.size);

	if (av_write_frame(p_audio_play->output_format_context, &packet) < 0) {
		fprintf(stderr, "av_write_frame()\n");
		exit(1);
	}

	//printf("==========================>%d %p\n", __LINE__, hd);
	av_free_packet(&packet);
#endif

	return 0;
}
