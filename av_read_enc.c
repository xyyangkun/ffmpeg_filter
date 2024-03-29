/******************************************************************************
 *
 *       Filename:  av_read_enc.c
 *
 *    Description:  av read video video and encoder
 *    https://cloud.tencent.com/developer/article/1932708
 *
 *    sudo apt-get install libavformat-dev libavutil-dev libavcodec-dev libswscale-dev
 *
 *    gcc av_read_enc.c  -lavformat -lavcodec -lavutil -lswscale -lswresample -lasound
 *
 *        Version:  1.0
 *        Created:  2023年02月28日 21时10分18秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <signal.h>
#include <pthread.h>


//推流的服务器地址
#define RTMP_SERVE_RADDR  "rtmp://js.live-send.acg.tv/live-js/?streamname=live_68130189_71037877&key=b95d4cfda0c196518f104839fe5e7573"

#define STREAM_DURATION   10.0   /*录制10秒的视频，由于缓冲的原因，一般只有8秒*/
#define STREAM_FRAME_RATE 15     /* 15 images/s   avfilter_get_by_name */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */
#define SCALE_FLAGS SWS_BICUBIC

//固定摄像头输出画面的尺寸
#define VIDEO_WIDTH  640
#define VIDEO_HEIGHT 480

//存放从摄像头读出转换之后的数据
unsigned char YUV420P_Buffer[VIDEO_WIDTH*VIDEO_HEIGHT*3/2];
unsigned char YUV420P_Buffer_temp[VIDEO_WIDTH*VIDEO_HEIGHT*3/2];

/*一些摄像头需要使用的全局变量*/
unsigned char *image_buffer[4];
int video_fd;
pthread_mutex_t mutex;
pthread_cond_t cond;

/*一些audio需要使用的全局变量*/
pthread_mutex_t mutex_audio;

extern int capture_audio_data_init( char *audio_dev);
extern int capture_audio_data(snd_pcm_t *capture_handle,int buffer_frames);
/*
 进行音频采集，采集pcm数据并直接保存pcm数据
 音频参数： 
	 声道数：		2
	 采样位数：	16bit、LE格式
	 采样频率：	44100Hz
*/
#define AudioFormat SND_PCM_FORMAT_S16_LE  //指定音频的格式,其他常用格式：SND_PCM_FORMAT_U24_LE、SND_PCM_FORMAT_U32_LE
#define AUDIO_CHANNEL_SET   1  			  //1单声道   2立体声
#define AUDIO_RATE_SET 44100   //音频采样率,常用的采样频率: 44100Hz 、16000HZ、8000HZ、48000HZ、22050HZ
FILE *pcm_data_file=NULL;

int buffer_frames;
snd_pcm_t *capture_handle;
snd_pcm_format_t format=AudioFormat;


//保存音频数据链表
struct AUDIO_DATA
{
	unsigned char* audio_buffer;
	struct AUDIO_DATA *next;
};

//定义一个链表头
struct AUDIO_DATA *list_head=NULL;
struct AUDIO_DATA *List_CreateHead(struct AUDIO_DATA *head);
void List_AddNode(struct AUDIO_DATA *head,unsigned char* audio_buffer);
void List_DelNode(struct AUDIO_DATA *head,unsigned char* audio_buffer);
int List_GetNodeCnt(struct AUDIO_DATA *head);

// 单个输出AVStream的包装器
typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc;

    /* 下一帧的点数*/
    int64_t next_pts;
    int samples_count;

    AVFrame *frame;
    AVFrame *tmp_frame;

    float t, tincr, tincr2;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
} OutputStream;


static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
    /*将输出数据包时间戳值从编解码器重新调整为流时基 */
    av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;
		
	/*将压缩的帧写入媒体文件*/
    return av_interleaved_write_frame(fmt_ctx, pkt);
}


/* 添加输出流。 */
static void add_stream(OutputStream *ost, AVFormatContext *oc,
                       AVCodec **codec,
                       enum AVCodecID codec_id)
{
    AVCodecContext *c;
    int i;

    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        exit(1);
    }

    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }
    ost->st->id = oc->nb_streams-1;
    c = avcodec_alloc_context3(*codec);
    if (!c) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        exit(1);
    }
    ost->enc = c;

    switch ((*codec)->type) {
    case AVMEDIA_TYPE_AUDIO:
        c->sample_fmt  = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        c->bit_rate    = 64000;  //设置码率
        c->sample_rate = 44100;  //音频采样率
        c->channels= av_get_channel_layout_nb_channels(c->channel_layout);
        c->channel_layout = AV_CH_LAYOUT_MONO; //AV_CH_LAYOUT_MONO 单声道   AV_CH_LAYOUT_STEREO 立体声
        c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
        ost->st->time_base = (AVRational){ 1, c->sample_rate };
        break;

    case AVMEDIA_TYPE_VIDEO:
        c->codec_id = codec_id;
		//码率：影响体积，与体积成正比：码率越大，体积越大；码率越小，体积越小。
        c->bit_rate = 400000; //设置码率 400kps
        /*分辨率必须是2的倍数。 */
        c->width    =VIDEO_WIDTH;
        c->height   = VIDEO_HEIGHT;
        /*时基：这是基本的时间单位（以秒为单位）
		 *表示其中的帧时间戳。 对于固定fps内容，
		 *时基应为1 / framerate，时间戳增量应为
		 *等于1。*/
        ost->st->time_base = (AVRational){1,STREAM_FRAME_RATE};
        c->time_base       = ost->st->time_base;
        c->gop_size      = 12; /* 最多每十二帧发射一帧内帧 */
        c->pix_fmt       = STREAM_PIX_FMT;
        c->max_b_frames = 0;  //不要B帧
        if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) 
		{
            c->mb_decision = 2;
        }
    break;

    default:
        break;
    }

    /* 某些格式希望流头分开。 */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

/**************************************************************/
/* audio output */

static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
                                  uint64_t channel_layout,
                                  int sample_rate, int nb_samples)
{
    AVFrame *frame = av_frame_alloc();
    frame->format = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;
    if(nb_samples)
	{
        av_frame_get_buffer(frame, 0);
    }
    return frame;
}

static void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
    AVCodecContext *c;
    int nb_samples;
    int ret;
    AVDictionary *opt = NULL;
    c = ost->enc;
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
	

    /*下面3行代码是为了生成虚拟的声音设置的频率参数*/
    ost->t     = 0;
    ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
    ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

	//AAC编码这里就固定为1024
    nb_samples = c->frame_size;

    ost->frame     = alloc_audio_frame(c->sample_fmt, c->channel_layout,
                                       c->sample_rate, nb_samples);
    ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
                                       c->sample_rate, nb_samples);

    /* copy the stream parameters to the muxer */
    avcodec_parameters_from_context(ost->st->codecpar, c);

    /* create resampler context */
    ost->swr_ctx = swr_alloc();

	/* set options */
    printf("c->channels=%d\n",c->channels);
	av_opt_set_int       (ost->swr_ctx, "in_channel_count",   c->channels,       0);
	av_opt_set_int       (ost->swr_ctx, "in_sample_rate",     c->sample_rate,    0);
	av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
	av_opt_set_int       (ost->swr_ctx, "out_channel_count",  c->channels,       0);
	av_opt_set_int       (ost->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
	av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);
    
	/* initialize the resampling context */
	swr_init(ost->swr_ctx);
}

/* 毫秒级 延时 */
void Sleep(int ms)
{
	struct timeval delay;
	delay.tv_sec = 0;
	delay.tv_usec = ms * 1000; // 20 ms
	select(0, NULL, NULL, NULL, &delay);
}


/*
准备虚拟音频帧
这里可以替换成从声卡读取的PCM数据
*/
static AVFrame *get_audio_frame(OutputStream *ost)
{
    AVFrame *frame = ost->tmp_frame;
    int j, i, v;
    int16_t *q = (int16_t*)frame->data[0];
    /* 检查我们是否要生成更多帧，用于判断是否结束*/
    if (av_compare_ts(ost->next_pts, ost->enc->time_base,STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
        return NULL;

   #if 1
	//获取链表节点数量
	int cnt=0;
	while(cnt<=0)
	{
		cnt=List_GetNodeCnt(list_head);
	}
	
	pthread_mutex_lock(&mutex_audio); /*互斥锁上锁*/

	//得到节点数据
	struct AUDIO_DATA *tmp=list_head;
	unsigned char *buffer;

	tmp=tmp->next;
	if(tmp==NULL)
	{
		printf("数据为NULL.\n");
		exit(0);
	}
	buffer=tmp->audio_buffer;
	
	//1024*16*1
	memcpy(q,buffer,frame->nb_samples*sizeof(int16_t)*ost->enc->channels);//将音频数据拷贝进入frame缓冲区
	
	List_DelNode(list_head,buffer);
	free(buffer);			
    pthread_mutex_unlock(&mutex_audio); /*互斥锁解锁*/
	#endif
	
    frame->pts = ost->next_pts;
    ost->next_pts  += frame->nb_samples;
    return frame;
}


/*
 *编码一个音频帧并将其发送到多路复用器
 *编码完成后返回1，否则返回0
 */
static int write_audio_frame(AVFormatContext *oc, OutputStream *ost)
{
    AVCodecContext *c;
    AVPacket pkt = { 0 };
    AVFrame *frame;
    int ret;
    int got_packet;
    int dst_nb_samples;

    av_init_packet(&pkt);
    c = ost->enc;

    frame = get_audio_frame(ost);

    if(frame)
	{
        /*使用重采样器将样本从本机格式转换为目标编解码器格式*/
		 /*计算样本的目标数量*/
		dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
										c->sample_rate, c->sample_rate, AV_ROUND_UP);
		av_assert0(dst_nb_samples == frame->nb_samples);
        av_frame_make_writable(ost->frame);
        /*转换为目标格式 */
        swr_convert(ost->swr_ctx,
                    ost->frame->data, dst_nb_samples,
                    (const uint8_t **)frame->data, frame->nb_samples);
        frame = ost->frame;
        frame->pts = av_rescale_q(ost->samples_count, (AVRational){1, c->sample_rate}, c->time_base);
        ost->samples_count += dst_nb_samples;
    }

    avcodec_encode_audio2(c, &pkt, frame, &got_packet);

    if (got_packet) 
	{
        write_frame(oc, &c->time_base, ost->st, &pkt);
    }
    return (frame || got_packet) ? 0 : 1;
}


static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
    AVFrame *picture;
    int ret;
    picture = av_frame_alloc();
    picture->format = pix_fmt;
    picture->width  = width;
    picture->height = height;

    /* allocate the buffers for the frame data */
    av_frame_get_buffer(picture, 32);
    return picture;
}


static void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
    AVCodecContext *c = ost->enc;
    AVDictionary *opt = NULL;
    av_dict_copy(&opt, opt_arg, 0);
    /* open the codec */
    avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    /* allocate and init a re-usable frame */
    ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
    ost->tmp_frame = NULL;
    /* 将流参数复制到多路复用器 */
    avcodec_parameters_from_context(ost->st->codecpar, c);
}


/*
准备图像数据
YUV422占用内存空间 = w * h * 2
YUV420占用内存空间 = width*height*3/2
*/
static void fill_yuv_image(AVFrame *pict, int frame_index,int width, int height)
{
	int y_size=width*height;
	/*等待条件成立*/
	pthread_mutex_lock(&mutex);
    pthread_cond_wait(&cond,&mutex);
	memcpy(YUV420P_Buffer_temp,YUV420P_Buffer,sizeof(YUV420P_Buffer));
	/*互斥锁解锁*/
	pthread_mutex_unlock(&mutex);
	
    //将YUV数据拷贝到缓冲区  y_size=wXh
	memcpy(pict->data[0],YUV420P_Buffer_temp,y_size);
	memcpy(pict->data[1],YUV420P_Buffer_temp+y_size,y_size/4);
	memcpy(pict->data[2],YUV420P_Buffer_temp+y_size+y_size/4,y_size/4);
}


static AVFrame *get_video_frame(OutputStream *ost)
{
    AVCodecContext *c = ost->enc;

    /* 检查我们是否要生成更多帧---判断是否结束录制 */
      if(av_compare_ts(ost->next_pts, c->time_base,STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
        return NULL;

    /*当我们将帧传递给编码器时，它可能会保留对它的引用
    *内部； 确保我们在这里不覆盖它*/
    if (av_frame_make_writable(ost->frame) < 0)
        exit(1);

	//制作虚拟图像
	//DTS（解码时间戳）和PTS（显示时间戳）
    fill_yuv_image(ost->frame, ost->next_pts, c->width, c->height);
    ost->frame->pts = ost->next_pts++;
    return ost->frame;
}

/*
*编码一个视频帧并将其发送到多路复用器
*编码完成后返回1，否则返回0
*/
static int write_video_frame(AVFormatContext *oc, OutputStream *ost)
{
    int ret;
    AVCodecContext *c;
    AVFrame *frame;
    int got_packet = 0;
    AVPacket pkt = { 0 };
    c=ost->enc;
	//获取一帧数据
    frame = get_video_frame(ost);
    av_init_packet(&pkt);

    /* 编码图像 */
    ret=avcodec_encode_video2(c, &pkt, frame, &got_packet);

    if(got_packet) 
	{
        ret=write_frame(oc, &c->time_base, ost->st, &pkt);
    }
	else
    {
        ret = 0;
    }
    return (frame || got_packet) ? 0 : 1;
}


static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}


//编码视频和音频
int video_audio_encode(char *filename)
{
    OutputStream video_st = { 0 }, audio_st = { 0 };
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVCodec *audio_codec, *video_codec;
    int ret;
    int have_video = 0, have_audio = 0;
    int encode_video = 0, encode_audio = 0;
    AVDictionary *opt = NULL;
    int i;

    /* 分配输出环境*/
    avformat_alloc_output_context2(&oc,NULL,"flv",filename);
    fmt=oc->oformat;
	//指定编码器
	fmt->video_codec=AV_CODEC_ID_H264;
	fmt->audio_codec=AV_CODEC_ID_AAC;
	
		 /*使用默认格式的编解码器添加音频和视频流，初始化编解码器。 */
    if(fmt->video_codec != AV_CODEC_ID_NONE)
	{
        add_stream(&video_st,oc,&video_codec,fmt->video_codec);
        have_video = 1;
        encode_video = 1;
    }
    if(fmt->audio_codec != AV_CODEC_ID_NONE)
	{
        add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
        have_audio = 1;
        encode_audio = 1;
    }

  /*现在已经设置了所有参数，可以打开音频视频编解码器，并分配必要的编码缓冲区。 */
    if (have_video)
        open_video(oc, video_codec, &video_st, opt);

    if (have_audio)
        open_audio(oc, audio_codec, &audio_st, opt);

    av_dump_format(oc, 0, filename, 1);

    /* 打开输出文件（如果需要） */
    if(!(fmt->flags & AVFMT_NOFILE)) 
	{
        ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0)
		{
            fprintf(stderr, "无法打开输出文件: '%s': %s\n", filename,av_err2str(ret));
            return 1;
        }
    }

    /* 编写流头（如果有）*/
    avformat_write_header(oc,&opt);

    while(encode_video || encode_audio)
	{
        /* 选择要编码的流*/
        if(encode_video &&(!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base,audio_st.next_pts, audio_st.enc->time_base) <= 0))
        {
			//printf("视频编码一次----->\n");
            encode_video = !write_video_frame(oc,&video_st);
        }
		else 
		{
			//printf("音频编码一次----->\n");
            encode_audio = !write_audio_frame(oc,&audio_st);
        }
    }
	
    av_write_trailer(oc);
	
    if (have_video)
        close_stream(oc, &video_st);
    if (have_audio)
        close_stream(oc, &audio_st);

    if (!(fmt->flags & AVFMT_NOFILE))
        avio_closep(&oc->pb);
    avformat_free_context(oc);
    return 0;
}


/*
函数功能: 摄像头设备初始化
*/
int VideoDeviceInit(char *DEVICE_NAME)
{
	/*1. 打开摄像头设备*/
	video_fd=open(DEVICE_NAME,O_RDWR);
	if(video_fd<0)return -1;

	/*2. 设置摄像头支持的颜色格式和输出的图像尺寸*/
	struct v4l2_format video_formt;
	memset(&video_formt,0,sizeof(struct v4l2_format));	
	video_formt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*视频捕获设备*/
	video_formt.fmt.pix.height=VIDEO_HEIGHT; //480 
	video_formt.fmt.pix.width=VIDEO_WIDTH; //640
	video_formt.fmt.pix.pixelformat=V4L2_PIX_FMT_YUYV;
	if(ioctl(video_fd,VIDIOC_S_FMT,&video_formt))return -2;
	printf("当前摄像头尺寸:width*height=%d*%d\n",video_formt.fmt.pix.width,video_formt.fmt.pix.height);
	
	/*3.请求申请缓冲区的数量*/
	struct v4l2_requestbuffers video_requestbuffers;
	memset(&video_requestbuffers,0,sizeof(struct v4l2_requestbuffers));	
	video_requestbuffers.count=4;
	video_requestbuffers.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*视频捕获设备*/
	video_requestbuffers.memory=V4L2_MEMORY_MMAP;
	if(ioctl(video_fd,VIDIOC_REQBUFS,&video_requestbuffers))return -3;
	printf("video_requestbuffers.count=%d\n",video_requestbuffers.count);

	/*4. 获取缓冲区的首地址*/
	struct v4l2_buffer video_buffer;
	memset(&video_buffer,0,sizeof(struct v4l2_buffer));
	int i;
	for(i=0;i<video_requestbuffers.count;i++)
	{
		video_buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*视频捕获设备*/
		video_buffer.memory=V4L2_MEMORY_MMAP;
		video_buffer.index=i;/*缓冲区的编号*/
		if(ioctl(video_fd,VIDIOC_QUERYBUF,&video_buffer))return -4;
		/*映射地址*/
		image_buffer[i]=mmap(NULL,video_buffer.length,PROT_READ|PROT_WRITE,MAP_SHARED,video_fd,video_buffer.m.offset);
		printf("image_buffer[%d]=0x%X\n",i,image_buffer[i]);
	}
	/*5. 将缓冲区加入到采集队列*/
	memset(&video_buffer,0,sizeof(struct v4l2_buffer));
	for(i=0;i<video_requestbuffers.count;i++)
	{
		video_buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*视频捕获设备*/
		video_buffer.memory=V4L2_MEMORY_MMAP;
		video_buffer.index=i;/*缓冲区的编号*/
		if(ioctl(video_fd,VIDIOC_QBUF,&video_buffer))return -5;
	}
	/*6. 启动采集队列*/
	int opt=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*视频捕获设备*/
	if(ioctl(video_fd,VIDIOC_STREAMON,&opt))return -6;
	return 0;
}


//YUYV==YUV422
int yuyv_to_yuv420p(const unsigned char *in, unsigned char *out, unsigned int width, unsigned int height)
{
    unsigned char *y = out;
    unsigned char *u = out + width*height;
    unsigned char *v = out + width*height + width*height/4;
    unsigned int i,j;
    unsigned int base_h;
    unsigned int  is_u = 1;
    unsigned int y_index = 0, u_index = 0, v_index = 0;
    unsigned long yuv422_length = 2 * width * height;
    //序列为YU YV YU YV，一个yuv422帧的长度 width * height * 2 个字节
    //丢弃偶数行 u v
    for(i=0; i<yuv422_length; i+=2)
    {
        *(y+y_index) = *(in+i);
        y_index++;
    }
    for(i=0; i<height; i+=2)
    {
        base_h = i*width*2;
        for(j=base_h+1; j<base_h+width*2; j+=2)
        {
            if(is_u)
            {
				*(u+u_index) = *(in+j);
				u_index++;
				is_u = 0;
            }
            else
            {
                *(v+v_index) = *(in+j);
                v_index++;
                is_u = 1;
            }
        }
    }
    return 1;
}


/*
子线程函数: 采集摄像头的数据
*/
void *pthread_read_video_data(void *arg)
{
	/*1. 循环读取摄像头采集的数据*/
	struct pollfd fds;
	fds.fd=video_fd;
	fds.events=POLLIN;

	/*2. 申请存放JPG的数据空间*/
	struct v4l2_buffer video_buffer;
	while(1)
	{
		 /*(1)等待摄像头采集数据*/
		 poll(&fds,1,-1);
		 /*(2)取出队列里采集完毕的缓冲区*/
		 video_buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*视频捕获设备*/
		 video_buffer.memory=V4L2_MEMORY_MMAP;
		 ioctl(video_fd,VIDIOC_DQBUF,&video_buffer);
         /*(3)处理图像数据*/
		 /*YUYV数据转YUV420P*/
		 pthread_mutex_lock(&mutex);   /*互斥锁上锁*/
		 yuyv_to_yuv420p(image_buffer[video_buffer.index],YUV420P_Buffer,VIDEO_WIDTH,VIDEO_HEIGHT);
		 pthread_mutex_unlock(&mutex); /*互斥锁解锁*/
		 pthread_cond_broadcast(&cond);/*广播方式唤醒休眠的线程*/
		 
		 /*(4)将缓冲区再放入队列*/
		 ioctl(video_fd,VIDIOC_QBUF,&video_buffer);
	}	
}

/*
子线程函数: 采集摄像头的数据
*/
void *pthread_read_audio_data(void *arg)
{
    capture_audio_data(capture_handle,buffer_frames);
}

//运行示例:  ./a.out /dev/video0
int main(int argc,char **argv)
{
	if(argc!=3)
	{
		printf("./app </dev/videoX> <hw:X> \n");
		return 0;
	}
	int err;
	pthread_t thread_id;
	
	//创建链表头
	list_head=List_CreateHead(list_head);
	
	/*初始化互斥锁*/
	pthread_mutex_init(&mutex,NULL);
	/*初始化条件变量*/
	pthread_cond_init(&cond,NULL);

    /*初始化互斥锁*/
	pthread_mutex_init(&mutex_audio,NULL);

	/*初始化摄像头设备*/
	err=VideoDeviceInit(argv[1]);
	printf("VideoDeviceInit=%d\n",err);
	if(err!=0)return err;
	/*创建子线程: 采集摄像头的数据*/
	pthread_create(&thread_id,NULL,pthread_read_video_data,NULL);
	/*设置线程的分离属性: 采集摄像头的数据*/
	pthread_detach(thread_id);

    capture_audio_data_init( argv[2]);
    /*创建子线程: 采集音频的数据*/
	pthread_create(&thread_id,NULL,pthread_read_audio_data,NULL);
	/*设置线程的分离属性: 采集摄像头的数据*/
	pthread_detach(thread_id);
	
	char filename[100];
	time_t t;
	struct tm *tme;
	//开始音频、视频编码
	while(1)
	{
		//开始视频编码
		video_audio_encode(RTMP_SERVE_RADDR);
	}
	return 0;
}

/*
函数功能： 创建链表头
*/
struct AUDIO_DATA *List_CreateHead(struct AUDIO_DATA *head)
{
	if(head==NULL)
	{
		head=malloc(sizeof(struct AUDIO_DATA));
		head->next=NULL;
	}
	return head;
}

/*
函数功能: 插入新的节点
*/
void List_AddNode(struct AUDIO_DATA *head,unsigned char* audio_buffer)
{
	struct AUDIO_DATA *tmp=head;
	struct AUDIO_DATA *new_node;
	/*找到链表尾部*/
	while(tmp->next)
	{
		tmp=tmp->next;
	}
	/*插入新的节点*/
	new_node=malloc(sizeof(struct AUDIO_DATA));
	new_node->audio_buffer=audio_buffer;
	new_node->next=NULL;
	/*将新节点接入到链表*/
	tmp->next=new_node;
}

/*
函数功能:删除节点
*/
void List_DelNode(struct AUDIO_DATA *head,unsigned char* audio_buffer)
{
	struct AUDIO_DATA *tmp=head;
	struct AUDIO_DATA *p;
	/*找到链表中要删除的节点*/
	while(tmp->next)
	{
		p=tmp;
		tmp=tmp->next;
		if(tmp->audio_buffer==audio_buffer)
		{
			p->next=tmp->next;
			free(tmp);
		}
	}
}

/*

*/


/*
函数功能:遍历链表，得到节点总数量
*/
int List_GetNodeCnt(struct AUDIO_DATA *head)
{
	int cnt=0;
	struct AUDIO_DATA *tmp=head;
	while(tmp->next)
	{
		tmp=tmp->next;
		cnt++;
	}
	return cnt;
}


int capture_audio_data_init( char *audio_dev)
{
	int i;
	int err;
	
	buffer_frames = 1024;
	unsigned int rate = AUDIO_RATE_SET;// 常用的采样频率: 44100Hz 、16000HZ、8000HZ、48000HZ、22050HZ
	capture_handle;// 一个指向PCM设备的句柄
	snd_pcm_hw_params_t *hw_params; //此结构包含有关硬件的信息，可用于指定PCM流的配置
	
	/*注册信号捕获退出接口*/
	printf("进入main\n");
	/*PCM的采样格式在pcm.h文件里有定义*/
	format=SND_PCM_FORMAT_S16_LE; // 采样位数：16bit、LE格式
	
	/*打开音频采集卡硬件，并判断硬件是否打开成功，若打开失败则打印出错误提示*/
	if ((err = snd_pcm_open (&capture_handle, audio_dev,SND_PCM_STREAM_CAPTURE,0))<0) 
	{
		printf("无法打开音频设备: %s (%s)\n",  audio_dev,snd_strerror (err));
		exit(1);
	}
	printf("音频接口打开成功.\n");
	
 
	/*分配硬件参数结构对象，并判断是否分配成功*/
	if((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) 
	{
		printf("无法分配硬件参数结构 (%s)\n",snd_strerror(err));
		exit(1);
	}
	printf("硬件参数结构已分配成功.\n");
	
	/*按照默认设置对硬件对象进行设置，并判断是否设置成功*/
	if((err=snd_pcm_hw_params_any(capture_handle,hw_params)) < 0) 
	{
		printf("无法初始化硬件参数结构 (%s)\n", snd_strerror(err));
		exit(1);
	}
	printf("硬件参数结构初始化成功.\n");
 
	/*
		设置数据为交叉模式，并判断是否设置成功
		interleaved/non interleaved:交叉/非交叉模式。
		表示在多声道数据传输的过程中是采样交叉的模式还是非交叉的模式。
		对多声道数据，如果采样交叉模式，使用一块buffer即可，其中各声道的数据交叉传输；
		如果使用非交叉模式，需要为各声道分别分配一个buffer，各声道数据分别传输。
	*/
	if((err = snd_pcm_hw_params_set_access (capture_handle,hw_params,SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) 
	{
		printf("无法设置访问类型(%s)\n",snd_strerror(err));
		exit(1);
	}
	printf("访问类型设置成功.\n");
 
	/*设置数据编码格式，并判断是否设置成功*/
	if ((err=snd_pcm_hw_params_set_format(capture_handle, hw_params,format)) < 0) 
	{
		printf("无法设置格式 (%s)\n",snd_strerror(err));
		exit(1);
	}
	fprintf(stdout, "PCM数据格式设置成功.\n");
 
	/*设置采样频率，并判断是否设置成功*/
	if((err=snd_pcm_hw_params_set_rate_near (capture_handle,hw_params,&rate,0))<0) 
	{
		printf("无法设置采样率(%s)\n",snd_strerror(err));
		exit(1);
	}
	printf("采样率设置成功\n");
 
	/*设置声道，并判断是否设置成功*/
	if((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params,AUDIO_CHANNEL_SET)) < -1) 
	{
		printf("无法设置声道数(%s)\n",snd_strerror(err));
		exit(1);
	}
	printf("声道数设置成功.\n");
 
	/*将配置写入驱动程序中，并判断是否配置成功*/
	if ((err=snd_pcm_hw_params (capture_handle,hw_params))<0) 
	{
		printf("无法向驱动程序设置参数(%s)\n",snd_strerror(err));
		exit(1);
	}
	printf("参数设置成功.\n");
	/*使采集卡处于空闲状态*/
	snd_pcm_hw_params_free(hw_params);
 
	/*准备音频接口,并判断是否准备好*/
	if((err=snd_pcm_prepare(capture_handle))<0) 
	{
		printf("无法使用音频接口 (%s)\n",snd_strerror(err));
		exit(1);
	}
	printf("音频接口准备好.\n");
	
	return 0;
}

unsigned char audio_read_buff[2048];
//音频采集线程
int capture_audio_data(snd_pcm_t *capture_handle,int buffer_frames)
{
	int err;
	//因为frame样本数固定为1024,而双通道，每个采样点2byte，所以一次要发送1024*2*2byte数据给frame->data[0];
	/*配置一个数据缓冲区用来缓冲数据*/
	//snd_pcm_format_width(format) 获取样本格式对应的大小(单位是:bit)
	int frame_byte=snd_pcm_format_width(format)/8;

	/*开始采集音频pcm数据*/
	printf("开始采集数据...\n");
	int i;
	char *audio_buffer;
	while(1) 
	{
		audio_buffer=malloc(buffer_frames*frame_byte*AUDIO_CHANNEL_SET); //2048
		if(audio_buffer==NULL)
		{
			printf("缓冲区分配错误.\n");
			break;
		}
		
		/*从声卡设备读取一帧音频数据:2048字节*/
		if((err=snd_pcm_readi(capture_handle,audio_read_buff,buffer_frames))!=buffer_frames) 
		{
			  printf("从音频接口读取失败(%s)\n",snd_strerror(err));
			  exit(1);
		}
	
		pthread_mutex_lock(&mutex_audio); /*互斥锁上锁*/
		memcpy(audio_buffer,audio_read_buff,buffer_frames*frame_byte*AUDIO_CHANNEL_SET);
		//添加节点
		List_AddNode(list_head,audio_buffer);
		pthread_mutex_unlock(&mutex_audio); /*互斥锁解锁*/
	}
 
	/*释放数据缓冲区*/
	free(audio_buffer);
 
	/*关闭音频采集卡硬件*/
	snd_pcm_close(capture_handle);
 
	/*关闭文件流*/
	fclose(pcm_data_file);

	return 0;
}
