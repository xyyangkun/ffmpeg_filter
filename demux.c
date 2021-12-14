/******************************************************************************
 *
 *       Filename:  demux.c
 *
 *    Description:  解析mp4文件
 *
 *        Version:  1.0
 *        Created:  2021年12月10日 10时10分45秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
/*!
* \file demux.c
* \brief 解析mp4文件
* 
* 
* \author yangkun
* \version 1.0
* \date 2021.12.10
*/


#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "demux.h"

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx=NULL;
static int width, height;
static enum AVPixelFormat pix_fmt;
static AVStream *video_stream = NULL, *audio_stream = NULL;
static const char *src_filename = NULL;
static const char *video_dst_filename = NULL;
static const char *audio_dst_filename = NULL;
static FILE *video_dst_file = NULL;
static FILE *audio_dst_file = NULL;

static uint8_t *video_dst_data[4] = {NULL};
static int      video_dst_linesize[4];
static int video_dst_bufsize;

static int video_stream_idx = -1, audio_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket pkt;
static AVPacket pkt1;
static int video_frame_count = 0;
static int audio_frame_count = 0;

// 音视频帧数据回调
static local_av_callback av_cb = NULL;

static local_av_callback_v1 audio_cb = NULL;
static local_av_callback_v1 video_cb = NULL;


static local_av_close_callback close_cb = NULL;

static pthread_t th_mux_read_proc;

static int is_call_close = 0; // 关闭调用
static long cb_handle = 0;

AVBitStreamFilterContext* h264bsfc = NULL;

static int decode_packet(int *got_frame, int cached);


/* Enable or disable frame reference counting. You are not supposed to support
 * both paths in your application but pick the one most appropriate to your
 * needs. Look for the use of refcount in this example to see what are the
 * differences of API usage between them. */
static int refcount = 0;

static int decode_packet(int *got_frame, int cached)
{
    int ret = 0;
    int decoded = pkt.size;

    *got_frame = 0;

    if (pkt.stream_index == video_stream_idx) {
		// 得到h264/h265 视频帧数据，将视频帧数据传给回调函数
		// 两种解析MP4方法
		// https://blog.csdn.net/m0_37346206/article/details/94029945
		// https://blog.csdn.net/godvmxi/article/details/52875058
		// av_cb(buf, size, 1);
		 int a = av_bitstream_filter_filter(h264bsfc, 
				 fmt_ctx->streams[video_stream_idx]->codec, 
				 NULL, 
				 &pkt1.data, 
				 &pkt1.size, 
				 pkt.data, 
				 pkt.size, 
				 pkt.flags & AV_PKT_FLAG_KEY);
		 /*
		 printf("a=%d\n", a);
			 printf("new_pkt.data=%p %p %d %d\n",
				 pkt.data, pkt1.data,
				 pkt.size, pkt1.size);
				 */
		 if(a<0){
			printf("error to get bit stream\n");
			exit(-1);
		 }
#if 0
		 if(a>=0){
		 av_free_packet(&pkt);
		 pkt.data = new_pkt.data;
		 pkt.size = new_pkt.size;
		 }
#endif
		//av_cb(pkt.data, pkt.size, 2, cb_handle);
		av_cb(pkt1.data, pkt1.size, 2, cb_handle);
		if(video_cb) {
			video_cb((void*)&pkt1, cb_handle);
		}

		// 释放内存
		av_freep(&pkt1.data);
		av_packet_free_side_data(&pkt1);
		av_packet_unref(&pkt1);
		//av_free_packet(&pkt1);
		pkt1.data = NULL;
		pkt1.size = 0;

    } else if (pkt.stream_index == audio_stream_idx) {
		assert(audio_dec_ctx);
        /* decode audio frame */
        ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error decoding audio frame (%s)\n", av_err2str(ret));
            return ret;
        }
        /* Some audio decoders decode only part of the packet, and have to be
         * called again with the remainder of the packet data.
         * Sample: fate-suite/lossless-audio/luckynight-partial.shn
         * Also, some decoders might over-read the packet. */
        decoded = FFMIN(ret, pkt.size);

        if (*got_frame) {
            size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample(frame->format);
#if 0
            printf("audio_frame%s n:%d nb_samples:%d pts:%s\n",
                   cached ? "(cached)" : "",
                   audio_frame_count++, frame->nb_samples,
                   av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));

			printf("pts:%lld %lld %lld  time_base: num:%d, den:%d\n",
					frame->pts, 
					frame->pkt_pts, 
					frame->pkt_dts, 
					audio_dec_ctx->time_base.num, audio_dec_ctx->time_base.den);
#endif

            /* Write the raw audio data samples of the first plane. This works
             * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
             * most audio decoders output planar audio, which uses a separate
             * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
             * In other words, this code will write only the first audio channel
             * in these cases.
             * You should use libswresample or libavfilter to convert the frame
             * to packed data. */
            //fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
			av_cb(frame->extended_data[0], unpadded_linesize, 1, cb_handle);
			if(audio_cb) {
				audio_cb((void*)frame, cb_handle);
			}
        }
    }

    /* If we use frame reference counting, we own the data and need
     * to de-reference it when we don't use it anymore */
    if (*got_frame && refcount)
        av_frame_unref(frame);

    return decoded;
}

static int open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), src_filename);
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];

        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }

        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }

        /* Init the decoders, with or without reference counting */
        av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
        if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }

    return 0;
}

static int get_format_from_sample_fmt(const char **fmt,
                                      enum AVSampleFormat sample_fmt)
{
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
    } sample_fmt_entries[] = {
        { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
        { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
        { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
        { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
        { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;

    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }

    fprintf(stderr,
            "sample format %s is not supported as output format\n",
            av_get_sample_fmt_name(sample_fmt));
    return -1;
}

/**
 * @brief 读取本地mp4文件线程
 * @param 线程参数
 * @return NULL
 */
static void *mux_read_proc(void *param) {
	int ret;
	int got_frame;
    /* read frames from the file */
	while (av_read_frame(fmt_ctx, &pkt) >= 0 && is_call_close==0) {
		AVPacket orig_pkt = pkt;
		do {
			ret = decode_packet(&got_frame, 0);
			if (ret < 0)
				break;
			pkt.data += ret;
			pkt.size -= ret;
		} while (pkt.size > 0);
		av_packet_unref(&orig_pkt);
	}

	/* flush cached frames */
	pkt.data = NULL;
	pkt.size = 0;
	do {
		decode_packet(&got_frame, 1);
	} while (got_frame);

	printf("Demuxing succeeded.\n");

	if (video_stream) {
		printf("Play the output video file with the command:\n"
				"ffplay -f rawvideo -pix_fmt %s -video_size %dx%d %s\n",
				av_get_pix_fmt_name(pix_fmt), width, height,
				video_dst_filename);
	}
#if 1
	if (audio_stream) {
		enum AVSampleFormat sfmt = audio_dec_ctx->sample_fmt;
		int n_channels = audio_dec_ctx->channels;
		const char *fmt;

		if (av_sample_fmt_is_planar(sfmt)) {
			const char *packed = av_get_sample_fmt_name(sfmt);
			printf("Warning: the sample format the decoder produced is planar "
					"(%s). This example will output the first channel only.\n",
					packed ? packed : "?");
			sfmt = av_get_packed_sample_fmt(sfmt);
			n_channels = 1;
		}

		if ((ret = get_format_from_sample_fmt(&fmt, sfmt)) < 0)
			goto end;

		printf("Play the output audio file with the command:\n"
				"ffplay -f %s -ac %d -ar %d %s\n",
				fmt, n_channels, audio_dec_ctx->sample_rate,
				audio_dst_filename);
	}
#endif

end:
	// 读取文件结束，通过回调通知上层，文件结束事件
	assert(close_cb);
	if(close_cb) {
		close_cb(is_call_close, cb_handle);
	}
}



int set_local_file_cb_v1(local_av_callback_v1 _audio_cb, local_av_callback_v1 _video_cb, long handle) 
{
	audio_cb = _audio_cb;
	video_cb = _video_cb;
}

/**
 * @brief 打开本地文件
 * @param[in] filename 带路径的文件名
 * @return 0 success, other failed
 */
int open_local_file(char *filename, local_av_callback _cb, local_av_close_callback _close_cb, long handle)
{
	int ret;
	is_call_close = 0;
	cb_handle = 0;

	cb_handle = handle;

	assert(!av_cb);
	assert(!close_cb);
	av_cb = _cb;
	close_cb = _close_cb;

	/* open input file, and allocate format context */
	if (avformat_open_input(&fmt_ctx, filename, NULL, NULL) < 0) {
		fprintf(stderr, "Could not open source file %s\n", src_filename);
		return -1;
	}
	/* retrieve stream information */
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		fprintf(stderr, "Could not find stream information\n");
		return -2;
	}
	
	if (open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
		video_stream = fmt_ctx->streams[video_stream_idx];

		/* allocate image where the decoded image will be put */
		width = video_dec_ctx->width;
		height = video_dec_ctx->height;
		pix_fmt = video_dec_ctx->pix_fmt;
		printf("video width:%d height:%d \n", width, height);
	}

	if (open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
		audio_stream = fmt_ctx->streams[audio_stream_idx];

		enum AVSampleFormat sfmt = audio_dec_ctx->sample_fmt;
		int n_channels = audio_dec_ctx->channels;
		printf("channe:%d sfmt:%d\n", n_channels, sfmt);
	}

	/* dump input information to stderr */
	av_dump_format(fmt_ctx, 0, src_filename, 0);


	if (!audio_stream && !video_stream) {
		fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
		ret = 1;
		close_local_file();
		exit(-1);
		return -1;
	}

	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate frame\n");
		ret = AVERROR(ENOMEM);
		close_local_file();
		exit(-1);
		return -2;
	}

	/* initialize packet, set data to NULL, let the demuxer fill it */
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	av_init_packet(&pkt1);
	pkt1.data = NULL;
	pkt1.size = 0;

	h264bsfc =  av_bitstream_filter_init("h264_mp4toannexb"); 

	// 创建线程读取文件, 并将音视频数据放入回调
	ret = pthread_create(&th_mux_read_proc, NULL, mux_read_proc, NULL);	
	if(ret!= 0) {
		printf("error to create thread:%d errstr:%s\n", ret, strerror(errno));
		exit(1);
	}
	printf("1>1>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

	return 0;
}

/**
 * @brief 关闭本地文件
 * @return 0 success,other failed
 */
int close_local_file() {
	is_call_close = 1;

	printf("%s %d===================>\n", __FUNCTION__, __LINE__);
	// 等待线程退出
	pthread_join(th_mux_read_proc, NULL);

	printf("%s %d===================>\n", __FUNCTION__, __LINE__);
	
	// 置空回调函数
	av_cb = NULL;
	close_cb = NULL;

	printf("%s %d===================>\n", __FUNCTION__, __LINE__);

	// 释放ffmpeg内存
	if(video_dec_ctx) {
		avcodec_free_context(&video_dec_ctx);
		video_dec_ctx = NULL;
	}

	printf("%s %d===================>\n", __FUNCTION__, __LINE__);
	if(audio_dec_ctx) {
		avcodec_free_context(&audio_dec_ctx);
		audio_dec_ctx = NULL;
	}

	printf("%s %d===================>\n", __FUNCTION__, __LINE__);
	if(fmt_ctx) {
		avformat_close_input(&fmt_ctx);
		fmt_ctx = NULL;
	}

	printf("%s %d===================>\n", __FUNCTION__, __LINE__);
	cb_handle = 0;

	if(frame){
		av_frame_free(&frame);
		frame = NULL;
	}

	printf("%s %d===================>\n", __FUNCTION__, __LINE__);
	av_bitstream_filter_close(h264bsfc);
}



