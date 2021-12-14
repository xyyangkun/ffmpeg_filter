/******************************************************************************
 *
 *       Filename:  test_demux_audio_player.c
 *
 *    Description:  mp4解码后声音s32转成s16播放
 *
 *        Version:  1.0
 *        Created:  2021年12月13日 15时02分24秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
/*!
* \file test_demux_audio_player.c
* \brief 解析mp4文件,播放声音
* 
* 
* \author yangkun
* \version 1.0
* \date 2021.12.13
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

#include "libswresample/swresample.h"

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>

#include "sound_card_get.h"



#include "demux.h"

int is_start = 0;
void _signal(int signo)
{
	signal(SIGINT, SIG_IGN); 
	signal(SIGTERM, SIG_IGN);

	printf("get signal:%d\n", signo);

	is_start = 0;

}




AVFormatContext *output_format_context = NULL;
AVCodecContext *output_codec_context = NULL;
static AVAudioFifo *fifo1 = NULL;
static struct SwrContext *swr_ctx1 = NULL;


/** Initialize one audio frame for reading from the input file */
static int init_input_frame(AVFrame **frame)
{
    if (!(*frame = av_frame_alloc())) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate input frame\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}

// 声音播放线程
static void *audio_play_proc(void *param) {

	AVFrame *frame = NULL;
	int frame_size = 1024;
	const int alsa_out_samples = 1024;
	int ret;

	printf("%s %d===================>\n", __FUNCTION__, __LINE__);

	while(is_start) {
		//printf("%s %d===================>\n", __FUNCTION__, __LINE__);


		if(init_input_frame(&frame) > 0){
			printf("=============>error :%d\n", __LINE__);
			//goto end;
			exit(1);
		}
		frame->format         = AV_SAMPLE_FMT_S16;
		frame->channel_layout = AV_CH_LAYOUT_STEREO;
		frame->nb_samples     = frame_size;
		frame->sample_rate    = 48000;

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

		if (av_audio_fifo_read(fifo1, (void **)frame->data, frame_size) < frame_size) {
			fprintf(stderr, "Could not read data from FIFO, fifo1 file size:%d\n", av_audio_fifo_size(fifo1));
			usleep(20*1000);	
			if(is_start == 0) {
				printf("%d will exit\n", __LINE__);
				av_frame_free(&frame);
				goto end;
			}
			goto again;

			//return AVERROR_EXIT;
		}

		// 构建音频包将音频写入声卡
		{
			AVPacket packet;
			av_new_packet(&packet, alsa_out_samples*2*2);

			memcpy(packet.data, frame->data[0], packet.size);
			//printf("======================================> write audio card\n");
			if (av_write_frame(output_format_context, &packet) < 0) {
				fprintf(stderr, "av_write_frame()\n");
				exit(1);
			}

			av_free_packet(&packet);
		}

		// 释放音频帧
		av_frame_free(&frame);
	}
end:
	printf("audio_play_proc will exit\n");
	if(frame) {
		av_frame_free(&frame);
	}

}

pthread_t th_audio_play;
// 初始化声音播放
int init_audio_play() {
	int ret;
	// 创建fifo
	if (!(fifo1 = av_audio_fifo_alloc(AV_SAMPLE_FMT_S16,
					2, 1))) {
		fprintf(stderr, "Could not allocate FIFO\n");
		exit(1);
		return AVERROR(ENOMEM);
	}

	// 创建转化
	// MP4中的文件肯定是48k 立体声的
	{
		int64_t src_ch_layout = AV_CH_LAYOUT_STEREO, dst_ch_layout = AV_CH_LAYOUT_STEREO;
		int src_rate = 48000, dst_rate = 48000;
		enum AVSampleFormat src_sample_fmt = AV_SAMPLE_FMT_FLTP, dst_sample_fmt = AV_SAMPLE_FMT_S16;
		/* create resampler context */
		swr_ctx1 = swr_alloc();
		if (!swr_ctx1) {
			fprintf(stderr, "Could not allocate resampler context\n");
			ret = AVERROR(ENOMEM);
			//goto end;
			exit(1);
		}

		/* set options */
		av_opt_set_int(swr_ctx1, "in_channel_layout",    src_ch_layout, 0);
		av_opt_set_int(swr_ctx1, "in_sample_rate",       src_rate, 0);
		av_opt_set_sample_fmt(swr_ctx1, "in_sample_fmt", src_sample_fmt, 0);

		av_opt_set_int(swr_ctx1, "out_channel_layout",    dst_ch_layout, 0);
		av_opt_set_int(swr_ctx1, "out_sample_rate",       dst_rate, 0);
		av_opt_set_sample_fmt(swr_ctx1, "out_sample_fmt", dst_sample_fmt, 0);

		/* initialize the resampling context */
		if ((ret = swr_init(swr_ctx1)) < 0) {
			fprintf(stderr, "Failed to initialize the resampling context\n");
			ret = -1;
			//goto end;
			exit(1);
		}
	}

	//printf("%s %d===================>\n", __FUNCTION__, __LINE__);

	// 创建线程
	ret = pthread_create(&th_audio_play, NULL, &audio_play_proc, NULL);
	if(ret != 0) {
		printf("error to create thread!!\n");
		exit(1);
	}

	//printf("%s %d===================>\n", __FUNCTION__, __LINE__);

	return 0;
}

// 反初始化声音播放
int deinit_audio_play() {
	// 停止线程
	if(th_audio_play != 0) {
		pthread_join(th_audio_play, NULL);
		th_audio_play = 0;
	}
	
	// 释放转化handle
	if(swr_ctx1) {
		swr_free(&swr_ctx1);
		swr_ctx1 = NULL;
	}



	// 释放fifo
	if(fifo1) {
		av_audio_fifo_free(fifo1);
		fifo1 = NULL;
	}
	return 0;
}

#ifdef ARM
pthread_t th_video_play;

//  创建解码器，将解码的数据发送到vo
int init_video_play() {


}
#endif



static char *const get_error_text(const int error)
{
    static char error_buffer[255];
    av_strerror(error, error_buffer, sizeof(error_buffer));
    return error_buffer;
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
    //AVOutputFormat* fmt = av_guess_format("alsa", "hw:0", NULL); //  打开声卡0
    //AVOutputFormat* fmt = av_guess_format("alsa", "hw:1", NULL); //  打开声卡1
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
#if 1
	// alsa_enc.c  输出， alsa_dec.c 是输入
	//  libavdevice/alsa_enc.c  audio_write_header  libavdevice/alsa.c ff_alsa_open
	//  可以知道ctx->url没有设置， 使用默认default
    //fmt_ctx->url = "hw:1";
    //fmt_ctx->filename = "hw:1";
#else
	// 使用这个函数打开
	avformat_alloc_output_context2
#endif


    // add stream to format context
    AVStream* stream = avformat_new_stream(fmt_ctx, NULL);
    if (!stream) {
        fprintf(stderr, "avformat_new_stream()\n");
        exit(1);
    }

	const int in_channels = 2;
	const int in_samples = 1024;
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

#endif
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


static uint8_t **dst_data = NULL;
static int dst_nb_samples;
static int frame_samples = 1024;
static int src_rate = 48000, dst_rate = 48000;
static int dst_nb_channels =0;
static int dst_ch_layout = AV_CH_LAYOUT_STEREO;
static enum AVSampleFormat dst_sample_fmt = AV_SAMPLE_FMT_S16;
static int dst_linesize = 0;
static int max_dst_nb_samples;

int audio_write_init()
{
	int ret;
	dst_nb_samples = av_rescale_rnd(frame_samples, dst_rate, src_rate, AV_ROUND_UP);
	// 源和目的采样率相同，采样点应该也是相同的
	assert(dst_nb_samples = frame_samples);

	/* buffer is going to be directly written to a rawaudio file, no alignment */
	max_dst_nb_samples = dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);
	assert(dst_nb_channels == 2);
	// 根据格式分配内存
	ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,
			dst_nb_samples, dst_sample_fmt, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate destination samples\n");
		exit(1);
		//goto end;
	}


}

int audio_write_deinit()
{
	// 释放内存
	if (dst_data)
		av_freep(&dst_data[0]);
	av_freep(&dst_data);
}

// 将flap音频转成s16音频后写入fifo
int audio_wirte_fifo(AVFrame *frame) {
	int ret;

	int frame_size = frame->nb_samples;
	assert(fifo1);
	assert(frame_size > 0);

	dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx1, src_rate) +
			frame_size, dst_rate, src_rate, AV_ROUND_UP);
	if (dst_nb_samples > max_dst_nb_samples) {
		av_freep(&dst_data[0]);
		ret = av_samples_alloc(dst_data, &dst_linesize, dst_nb_channels,
				dst_nb_samples, dst_sample_fmt, 1);
		if (ret < 0) {
			fprintf(stderr, "Could not allocate destination samples\n");
			exit(1);
			//break;
		}
		max_dst_nb_samples = dst_nb_samples;
		printf("=========================> !! change!!:%d \n", max_dst_nb_samples);
	}	

	//printf("frame_size:%d dst_nb_samples:%d\n", frame_size, dst_nb_samples);
	//  先转换为48000 s16格式
	/* convert to destination format */
	//ret = swr_convert(swr_ctx1, dst_data, dst_nb_samples, (const uint8_t **)frame->data, frame_size);
	ret = swr_convert(swr_ctx1, dst_data, dst_nb_samples, (const uint8_t **)frame->data, frame_size);
	if (ret < 0) {
		fprintf(stderr, "Error while converting\n");
		exit(1);
		//goto end;
	}


	assert(dst_data != NULL);
	int size = frame_size;

#if 0
	printf("=========================> write audio data into fifo, av_audio_fifo_size=%d %d\n",
			av_audio_fifo_size(fifo1), av_audio_fifo_space(fifo1));
#endif
	if(av_audio_fifo_write(fifo1, (void **)dst_data, size) < size)
	{
		printf("fifo size:%d\n", av_audio_fifo_size(fifo1));
		fprintf(stderr, "cound not write data to fifo\n");
		exit(1);
	}

	if(av_audio_fifo_size(fifo1) > 1024*2) {
		usleep(20*1000);
	}

	if(av_audio_fifo_space(fifo1) > 1024*2) {
		usleep(10*1000);
	}




	return 0;
}







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

void audio_frame_write(void *buf_handle, long handle)
{
	//printf("in audio frame write will write fifo\n");
	audio_wirte_fifo((AVFrame *)buf_handle);
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

		//audio_wirte_fifo(AVFrame *frame);
	}

	// h264
	if(type == 2 && param->fp_video!= NULL)
	{
		//fwrite(head, 1, 4, param->fp_video);
		fwrite(buf, 1, buf_size, param->fp_video);
	}
}

char *path;
// MP4读取结束回调
void file_close(unsigned int type, long handle) {
	struct s_param *param = (struct s_param *)handle;
	printf("===========>mp4 file close , type=%d\n", type);
	//is_start = 0;
	if(is_start == 1)
	{

		// 关闭
		close_local_file();

		// 重新打开
		int ret = open_local_file(path, file_write, file_close, (long)&g_param);
		assert(ret == 0);
	}
}

//char *path = "/nfsroot/rk3568/rk3568_1080p_h264.mp4";
//char *path = "/nfsroot/rk3568/blackmagicdesign_p30.mp4";
char *path = "/nfsroot/rk3568/h264_720p_founders.mp4";

int main() {

    int err;

	is_start = 1;

    av_log_set_level(AV_LOG_VERBOSE);
	av_register_all();
	// 初始化网络库
	avformat_network_init();
	avfilter_register_all();
	avdevice_register_all();

	//show_banner();
	unsigned version = avcodec_version();
	printf("version:%d\n", version);

	audio_write_init();

	// 创建线程从线程读fifo写入声卡播放声音
	init_audio_play();

	char* outputFile = "alsa";
	err = open_output_alsa_file(outputFile, &output_format_context, &output_codec_context);
	printf("open output file err : %d\n", err);
	assert(output_format_context);
	av_dump_format(output_format_context, 0, outputFile, 1);
	
	if(write_output_file_header(output_format_context) < 0){
		av_log(NULL, AV_LOG_ERROR, "Error while writing header outputfile\n");
		exit(1);
	}


	set_local_file_cb_v1(audio_frame_write, NULL, (long)&g_param);
	
	int ret;
	ret = fp_init();
	assert(ret == 0);
	printf("1>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	ret = open_local_file(path, file_write, file_close, (long)&g_param);


	printf("2>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

	signal(SIGINT, _signal);
	signal(SIGTERM, _signal);


	while(is_start == 1) {
		sleep(1);
	}
	printf("%s %d===================>\n", __FUNCTION__, __LINE__);



	close_local_file();
	printf("%s %d===================>\n", __FUNCTION__, __LINE__);

	deinit_audio_play();
	printf("%s %d===================>\n", __FUNCTION__, __LINE__);


	audio_write_deinit();
	printf("%s %d===================>\n", __FUNCTION__, __LINE__);

	return 0;
}



