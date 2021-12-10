/******************************************************************************
 *
 *       Filename:  com.h
 *
 *    Description:  串口底层操作
 *
 *        Version:  1.0
 *        Created:  2020年04月14日 15时04分33秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  yangkun@osee-dig.com
 *        Company:  osee-dig.com
 *
 *****************************************************************************/
#ifndef _COM_H_
#define _COM_H_
#include <stdio.h>
//红色表示错误信息
#define com_error(fmt,args ...)    do {\
    printf("\033[1;31m%s %s ==>%s(%d)" ": " fmt"\033[0;39m", __DATE__,__TIME__,__FUNCTION__, __LINE__, ## args);\
}while(0)
//绿色提示信息
#define com_info(fmt,args ...) do {\
    printf("\033[1;33m"fmt"\033[0;39m", ## args);\
}while(0)

#define com_trace_err(fmt, arg...) do {\
    fprintf(stderr, "\33[31;1m%s, %s-%d: "fmt"\33[0m\n", __FILE__, __func__, __LINE__, ##arg);\
}while(0)

//绿色表示库里的提示信息
    /* printf("\033[1;32m%s(%d)" ": " fmt"\033[0;39m",__FUNCTION__, __LINE__, ## args);\ */
    /* printf("%s(%d)" ": " fmt"",__FUNCTION__, __LINE__, ## args);\  */

#ifdef DEBUG
#define com_print(fmt,args ...) do {\
	printf("%s(%d)" ": " fmt " ", __FUNCTION__, __LINE__, ## args);\
}while(0)

#else
#define com_print(fmt,args ...)
//#define com_error(fmt,args ...)
//#define com_info(fmt,args ...)
//#define com_trace_err(fmt, arg...)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// 串口操作中断类型
typedef enum CommPurgeFlags
{
    commPurgeTxAbort = 0x0001,  ///< 中止写操作
    commPurgeRxAbort = 0x0002,  ///< 中止读操作
    commPurgeTxClear = 0x0004,  ///< 清空输出缓冲
    commPurgeRxClear = 0x0008   ///< 清空输入缓冲
} CommPurgeFlags;

/// 串口停止位类型
typedef enum CommStopBit
{
    commOneStopBit = 0,     ///< 1 stop bit
    commOne5StopBits,       ///< 1.5 stop bit
    commTwoStopBits         //< 2 stop bit
} CommStopBit;

/// 串口校验位类型
typedef enum CommParityType
{
    commNoParity = 0,   ///< No parity
    commOddParity,      ///< Odd parity
    commEvenParity,     ///< Even parity
    commMarkParity,     ///< Mark parity
    commSpaceParity     ///< Space parity
} CommParityType;

/// 串口模式
typedef enum CommMode
{
    commFullDuplex = 0, ///< 全双工
    commSemiDuplex,     ///< 半双工
} CommMode;

/// 串口接口描述结构，128字节
typedef struct CommPortDesc
{
    int port;           ///< 物理端口号
    int reserved[31];   ///< 保留字节
} CommPortDesc;

/// 串口属性结构，128字节
typedef struct CommAttribute
{
    int     baudrate;   ///< 实际的波特率值。
    char    dataBits;   ///< 实际的数据位 取值为5,6,7,8
    char    parity;     ///< 奇偶校验选项，取CommParityType类型的枚举值。
    char    stopBits;   ///< 停止位数，取CommStopBit类型的枚举值。
    char    resv;       ///< 保留字节
    int     reserved[30];///< 保留字节
} CommAttribute;

struct COMMATTRI
{
    int iDataBits;  // 数据位取值为5,6,7,8
    int iStopBits;  // 停止位
    int iParity;    // 校验位
    int iBaudRate;  // 实际波特率
};
#define MAX_PROTOCOL_LENGTH 32
// 串口配置
struct CONFIG_COMM_X
{
    char sProtocolName[MAX_PROTOCOL_LENGTH];    // 串口协议:“Console”
    int iPortNo;        // 端口号
    struct COMMATTRI aCommAttri;       // 串口属性
};   

/// 串口属性结构
typedef struct COMM_ATTR
{
	unsigned int	baudrate;	///< 实际的波特率值。		
	unsigned char	databits;	///< 实际的数据位数。	
	unsigned char	parity;		///< 奇偶校验选项，取comm_parity_t类型的枚举值。	
	unsigned char	stopbits;	///< 停止位数，取comm_stopbits_t类型的枚举值。	
	unsigned char	reserved;	///< 保留	
} COMM_ATTR;


#ifdef __cplusplus
}
#endif

#endif//_COM_H_
