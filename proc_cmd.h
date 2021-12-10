/******************************************************************************
 *
 *       Filename:  proc_cmd.h
 *
 *    Description:  proc cmd
 *
 *        Version:  1.0
 *        Created:  2021年11月29日 20时24分40秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
#ifndef _PROC_CMD_H_
#define _PROC_CMD_H_

#include <map>
#include <vector>

#include <string>
#include <cstring>
#include <unistd.h>

#include <assert.h>
#include <signal.h>

//#include "video_audio_ctrl.h"

//#include "comm_service.h"
//#include "proc_cmd_cpp.h"

#include <sys/socket.h>
#include <netinet/in.h>

#define USE_UDP
#define NO_RETURN 0xffffffff

#define SERV_PORT 8888

// 如果命令解析失败返回错误，并退出之
class proc_cmd
{
public:
	proc_cmd();
	~proc_cmd(){close();};
	int close();


	int init();


	// 如果命令解析失败返回错误，并退出之
	int recv_cmd();

	int ret_cmd(int ret_value=0, char *data=NULL, unsigned int data_len=0);

	// command = 0x11 QT获取支持分辨率列表
	int proc_get_resolution(unsigned char *data, unsigned int data_len);

	// command = 0x12 QT修改分辨率
	int proc_change_resolution(unsigned char *data, unsigned int data_len);

	// command = 0x20 移动视频通道的位置
	//int proc_v_move(unsigned char *data, unsigned int data_len);
	// command = 0x2 QT发送虚拟坐席布局给HI


	int proc_change_volume(unsigned char *data, unsigned int data_len);

	// 检查信号中断线程
	void th_check_connect(void *param);

private:
	inline char *get_ip(unsigned int ip)
	{
		memset(ip_str, 0, sizeof(ip_str));
#if 1
		unsigned char *p = (unsigned char *)&ip;
#if 1
		sprintf(ip_str, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
#else
		sprintf(ip_str, "%d.%d.%d.%d", p[3], p[2], p[1], p[0]);
#endif
#endif
		return ip_str;
	}

	char ip_str[20];

	unsigned char group_id;
	unsigned char command_id;
	
	int sock;

private:
	// socket
	struct sockaddr_in servaddr, cliaddr;
	socklen_t cliaddr_len;

};

#endif//_PROC_CMD_H_


