/******************************************************************************
 *
 *       Filename:  usb_audio_aac.c
 *
 *    Description:  usb audio capture to aac
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
#define OUTPUT_BIT_RATE 16000
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

static AVFormatContext *ifmt_ctx = NULL;
pthread_t recv_id;
rtsp_cb cb;

struct s_out_file
{
	FILE* video;
	FILE* audio;
};

struct s_out_file out_file;


/**
 * Initialize one data packet for reading or writing.
 * @param packet Packet to be initialized
 */
static void init_packet(AVPacket *packet)
{
    av_init_packet(packet);
    /* Set the packet data and size so that it is recognized as being empty. */
    packet->data = NULL;
    packet->size = 0;
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
static int open_output_file(const char *filename,
                            //AVCodecContext *input_codec_context,
                            AVFormatContext **output_format_context,
                            AVCodecContext **output_codec_context)
{
    AVCodecContext *avctx          = NULL;
    AVIOContext *output_io_context = NULL;
    AVStream *stream               = NULL;
    AVCodec *output_codec          = NULL;
    int error;

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
        return AVERROR(ENOMEM);
    }

    /* Associate the output file (pointer) with the container format context. */
    (*output_format_context)->pb = output_io_context;

    /* Guess the desired container format based on the file extension. */
    if (!((*output_format_context)->oformat = av_guess_format(NULL, filename,
                                                              NULL))) {
        fprintf(stderr, "Could not find output file format\n");
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
    avctx->sample_rate    = 16000;////input_codec_context->sample_rate;
    avctx->sample_fmt     = output_codec->sample_fmts[0];
    avctx->bit_rate       = OUTPUT_BIT_RATE;

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
}
/**
 * Write the header of the output file container.
 * @param output_format_context Format context of the output file
 * @return Error code (0 if successful)
 */
static int write_output_file_header(AVFormatContext *output_format_context)
{
    int error;
    if ((error = avformat_write_header(output_format_context, NULL)) < 0) {
        fprintf(stderr, "Could not write output file header (error '%s')\n",
                av_err2str(error));
        return error;
    }
    return 0;
}

/**
 * Write the trailer of the output file container.
 * @param output_format_context Format context of the output file
 * @return Error code (0 if successful)
 */
static int write_output_file_trailer(AVFormatContext *output_format_context)
{
    int error;
    if ((error = av_write_trailer(output_format_context)) < 0) {
        fprintf(stderr, "Could not write output file trailer (error '%s')\n",
                av_err2str(error));
        return error;
    }
    return 0;
}

static int64_t pts = 0;

/**
 * Encode one frame worth of audio to the output file.
 * @param      frame                 Samples to be encoded
 * @param      output_format_context Format context of the output file
 * @param      output_codec_context  Codec context of the output file
 * @param[out] data_present          Indicates whether data has been
 *                                   encoded
 * @return Error code (0 if successful)
 */
static int encode_audio_frame(AVFrame *frame,
                              AVFormatContext *output_format_context,
                              AVCodecContext *output_codec_context,
                              int *data_present)
{
    /* Packet used for temporary storage. */
    AVPacket output_packet;
    int error;
    init_packet(&output_packet);

    /* Set a timestamp based on the sample rate for the container. */
    if (frame) {
        frame->pts = pts;
        pts += frame->nb_samples;
    }

    /* Send the audio frame stored in the temporary packet to the encoder.
     * The output audio stream encoder is used to do this. */
    error = avcodec_send_frame(output_codec_context, frame);
    /* The encoder signals that it has nothing more to encode. */
    if (error == AVERROR_EOF) {
        error = 0;
        goto cleanup;
    } else if (error < 0) {
        fprintf(stderr, "Could not send packet for encoding (error '%s')\n",
                av_err2str(error));
        return error;
    }

    /* Receive one encoded frame from the encoder. */
    error = avcodec_receive_packet(output_codec_context, &output_packet);
    /* If the encoder asks for more data to be able to provide an
     * encoded frame, return indicating that no data is present. */
    if (error == AVERROR(EAGAIN)) {
        error = 0;
        goto cleanup;
    /* If the last frame has been encoded, stop encoding. */
    } else if (error == AVERROR_EOF) {
        error = 0;
        goto cleanup;
    } else if (error < 0) {
        fprintf(stderr, "Could not encode frame (error '%s')\n",
                av_err2str(error));
        goto cleanup;
    /* Default case: Return encoded data. */
    } else {
        *data_present = 1;
    }

    /* Write one audio frame from the temporary packet to the output file. */
    if (*data_present &&
        (error = av_write_frame(output_format_context, &output_packet)) < 0) {
        fprintf(stderr, "Could not write frame (error '%s')\n",
                av_err2str(error));
        goto cleanup;
    }

cleanup:
    av_packet_unref(&output_packet);
    return error;
}


/**
 * Initialize one audio frame for reading from the input file.
 * @param[out] frame Frame to be initialized
 * @return Error code (0 if successful)
 */
static int init_input_frame(AVFrame **frame)
{
    if (!(*frame = av_frame_alloc())) {
        fprintf(stderr, "Could not allocate input frame\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}



void *recv_proc(void *param)
{
	int ret;
	AVPacket pkt;
	int data_present = 0;


	while(is_start) {
		ret = av_read_frame(ifmt_ctx, &pkt);
		if(ret < 0) {
			printf("error to read frame\n");
			break;
		}


		printf("pkt size:%d\n", pkt.size);
		// 构建编码帧，并进行编码
		{
			AVFrame *frame;
			const int frame_size = pkt.size;//256;
			int data_written;
			int error;

			/* Create a new frame to store the audio samples. */
			if (!(frame = av_frame_alloc())) {
				fprintf(stderr, "Could not allocate output frame\n");
				return AVERROR_EXIT;
			}

			/* Set the frame's parameters, especially its size and format.
			 * av_frame_get_buffer needs this to allocate memory for the
			 * audio samples of the frame.
			 * Default channel layouts based on the number of channels
			 * are assumed for simplicity. */
			frame->nb_samples     = frame_size;
			frame->channel_layout = output_codec_context->channel_layout;
			frame->format         = output_codec_context->sample_fmt;
			frame->sample_rate    = output_codec_context->sample_rate;

			/* Allocate the samples of the created frame. This call will make
			 * sure that the audio frame can hold as many samples as specified. */
			if ((error = av_frame_get_buffer(frame, 0)) < 0) {
				fprintf(stderr, "Could not allocate output frame samples (error '%s')\n",
						av_err2str(error));
				av_frame_free(&frame);
				return error;
			}



			/* Encode one frame worth of audio samples. */
			if (encode_audio_frame(frame, output_format_context,
						output_codec_context, &data_written)) {
				av_frame_free(&frame);
				return AVERROR_EXIT;
			}
			av_frame_free(&frame);

		}

		//av_free_packet(&pkt);
		av_packet_unref(&pkt);
	}


end:
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
		fp = fopen("audio_out.wav", "wb");
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

    av_log_set_level(AV_LOG_VERBOSE);

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

	char* outputFile = "output.aac";
    remove(outputFile);

	// 写音频文件
	err = open_output_file(outputFile, &output_format_context, &output_codec_context);
	printf("open output file err : %d\n", err);

	av_dump_format(output_format_context, 0, outputFile, 1);

#if 1
	// aac 不用写文件头
	if(write_output_file_header(output_format_context) < 0){
		av_log(NULL, AV_LOG_ERROR, "Error while writing header outputfile\n");
		exit(1);
	}
#endif


	
	ret = init_audio_capture(argv[1], argv[2]);
	if(ret != 0){
		printf("error init audio\n");
		return ret;
	}

	while(is_start){
		usleep(50*1000);
	}

#if 1
	if(write_output_file_trailer(output_format_context) < 0){
		av_log(NULL, AV_LOG_ERROR, "Error while writing header outputfile\n");
		exit(1);
	}
#endif

	deinit_audio_capture();

    avio_close((output_format_context)->pb);
    avformat_free_context(output_format_context);

	return 0;
}
