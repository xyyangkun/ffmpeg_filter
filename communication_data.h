/******************************************************************************
 *
 *       Filename:  communication_data.h
 *
 *    Description:  通信数据封装
 *
 *        Version:  1.0
 *        Created:  2020年10月09日 14时31分10秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
#ifndef _COMMUNICATION_DATA_H_
#define _COMMUNICATION_DATA_H_
//https://blog.csdn.net/u010144805/article/details/80803572

/**********************************************************
 * 通信数据封装
**********************************************************/
// 接收数据
#pragma pack(1)	// 一字节对齐
typedef struct s_recv_data
{
	unsigned char head1; 		// 0xeb
	unsigned char head2;		// 0xa6
	unsigned char group_id;		// 命令组id
	unsigned char command_id;	// 命令id
	unsigned short data_len;	// Data Length = len(data)+len(crc16)
	unsigned char data[];		// 具体数据
	// unsigned short crc16;	// 最后加上crc16
}t_recv_data;
#pragma pack () // 取消对齐


// 响应数据
#pragma pack(1)	// 一字节对齐
typedef struct s_resp_data
{
	unsigned char head1; 		// 0xeb
	unsigned char head2;		// 0xa6
	unsigned char group_id;		// 命令组id
	unsigned char command_id;	// 命令id
	unsigned short error_code;	// 错误代码
	unsigned short data_len;	// Data Length = len(data)+len(crc16)
	unsigned char data[];		// 具体数据
	// unsigned short crc16;	// 最后加上crc16
}t_resp_data;
#pragma pack () // 取消对齐

enum EB_EEROR
{
	EB_OK               = 0x00,	                      // 成功
	EB_FAILE            = 0x01,		                      // 命令失败
	EB_EMPTY_DATA       = 0x02,	                      // 空包
	EB_WRONG_HEAD       = 0x03,	                      // 包头错误 
	EB_WRONG_LEN        = 0x04,	                      // 包长度错误

	EB_CHECKSUM_FAIL    =0X5,                 // CRC16检查错误
	EB_WRONG_AUTH       =0x06,	                      // 用户登陆名或密码错误

	EB_PARAM_INVALID    = 0x0A,           // 参数错误, 指令无法识别
    EB_OPERATION_FAIL   = 0x0B,	          // 操作失败, 子卡执行出错
    EB_UNSUPPORT_CMD    = 0x0C,	          // 不支持的命令,  指令不支持
    EB_BUSY             = 0x07,   			      // 子卡忙等待
	EB_GET_LAYOUT       = 0x08,     	          // 用户布局失败

	EB_BIND_BIG_SCREEN   =0x100,				 // 绑定大屏出错

    EB_WRONG_CHN        = 0Xf10,         // 通道超限
    EB_WRONG_MULTICAST  = 0xf11,         // 组播信息有误
    EB_WRONG_OPERATE    = 0xf12,                 // 操作地址错误

    EB_NET_RECV_FAIL    = 0xf13,                 // 网络接收数据出错
    EB_NET_SEND_FAIL    = 0xf14,                 // 网络发送数据出错

    EB_AUDIO_BIND       = 0xf15,                 // 音频绑定类型错误
    EB_AUDIO_MUTE_TYPE  = 0xf16,                 // 音频静音类型错误
	EB_NO_AVP           = 0xf17,		   // 高成本带avp的盒子收到了低成本的协议命令
	EB_WRONG_SCREEN_NUM = 0xf18,		   // 划分屏数量不等于行*列数量 
	EB_WRONG_CONTENT_LEN= 0xf19,		   // 协议数据检测出错 



    EB_NO_RETURN = 0Xffffffff,  // 告诉后续处理函数，不用进行返回操作

};

#define CHECK_HEAD(data) \
	do { \
		if(data[0] != 0xeb || data[1] != 0xa6) return EB_WRONG_HEAD;\
	} while(0) 

/////////////////////  KM  /////////////////////////////////////
//http://www.ip33.com/crc.html
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
uint16_t crc16_calc(uint8_t *buf, int len)
{
    uint16_t i = 0, j = 0, temp = 0, CRC16 = 0;
    CRC16 = 0xFFFF;

    for(i = 0; i < len; i++)
    {
        CRC16 ^= buf[i];
        for(j = 0; j < 8; j++)
        {
            temp = (uint16_t)(CRC16 & 0x0001);
            CRC16 >>= 1;
            if(temp == 1)
            {
                CRC16 ^= 0xA001;
            }
        }
    }

    return CRC16;
}

// command = 0x22 音频配对
#pragma pack(1) // 一字节对齐
struct a_bind
{
    unsigned int ip; /// 音频源ip
    unsigned char type; //音频类型0 模拟 1hdmi
};
#pragma pack()

//0x101
#pragma pack(1) // 一字节对齐
struct set_volume
{
    unsigned short f; // 1
    unsigned short s; //2
    unsigned char type; //音频类型0 模拟 1hdmi
};
#pragma pack()



#endif//_COMMUNICATION_DATA_H_
