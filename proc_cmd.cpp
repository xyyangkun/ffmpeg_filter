/******************************************************************************
 *
 *       Filename:  proc_cmd.cpp
 *
 *    Description: 处理命令 
 *
 *        Version:  1.0
 *        Created:  2020年11月17日 19时12分58秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
#include <cstdio>
#include "communication_data.h"
#include "proc_cmd.h"

#ifdef ZLOG
	#include "zlog/zlog.h"
#endif



#ifndef ZLOG
#if 1
#define proc_dbg(fmt,args ...)do{ \
printf("\033[1;33m %d " fmt "\033[0;39m", __LINE__,  ## args);\
}while(0)

#define proc_err(fmt,args ...)do{ \
printf("\033[1;31m%s %s ==>%s(%d)" ": " fmt"\033[0;39m", __DATE__,__TIME__,__FUNCTION__, __LINE__, ## args); \
}while(0)
#else
#define proc_dbg(fmt,args ...)
#define proc_err(fmt,args ...)
#endif
#else // ZLOG
	/// ZLOG的实现
zlog_category_t *proc_cmd_c = NULL; 
#define proc_dbg(fmt,args ...)do{ \
	if(proc_cmd_c)zlog_debug(proc_cmd_c, fmt, ##args ); \
}while(0)

#define proc_err(fmt,args ...)do{ \
	if(proc_cmd_c)zlog_error(proc_cmd_c, fmt, ##args ); \
}while(0)

#endif // endof ZLOG

/***************************************************************************
 *  1 2 3 4 vo 为  信号源列表
 *  
 *  5 6 7 8 .... 16 vo 为虚拟坐席分割屏
 *
 * ************************************************************************/


#ifdef ZLOG
static void zlog_init()
{
	proc_cmd_c = zlog_get_category("zlog");
	if(!proc_cmd_c)
	{
		fprintf(stderr, "============================> Error in get comm protcol category\n");
		zlog_fini();
	}
}
#endif



proc_cmd::proc_cmd()
{
#ifdef ZLOG
	zlog_init();
	// test debug
	printf("will use zlog debug ===============================>\n");
	proc_dbg("use zlog debug!!!!===5 == %d\n", 55);
#endif


}

// 创建socket
int proc_cmd::init() 
{
	int sockfd;
	int i, n;
	int et;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0)
	{
		printf("ERROR to create socket:%d=>%s\n", errno, strerror(errno));
		exit(-1);
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    //inet_pton(AF_INET, "192.168.1.5", &servaddr.sin_addr); 
	servaddr.sin_port = htons(SERV_PORT);
    
	if( 0 != bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)))
	{
		printf("ERROR to bind:%d=>%s\n", errno, strerror(errno));
		exit(-2);
	}
	sock = sockfd;
	return 0;
}

// 回复视频返回值
int proc_cmd::ret_cmd(int ret_value/*=0*/, char *data, unsigned int data_len)
{
	std::string data_head;
	data_head.resize(sizeof(t_resp_data) + 2 + data_len);
	t_resp_data *head = (t_resp_data *)data_head.data();
	head->head1 = 0xeb;
	head->head2 = 0xa6;
	head->group_id = group_id;
	head->command_id = command_id;
	head->error_code = ret_value;
	head->data_len = data_len + 2;
	
	if(data_len != 0)
	{
		// 复制数据到head中
		memcpy((void *)(data_head.data() + sizeof(t_resp_data)), (void *)data, data_len);
	}

	// 计算crc16
	uint16_t *crc16 = (uint16_t *)( (char *)(data_head.data() + sizeof(t_resp_data)) + data_len);
	*crc16 = crc16_calc((uint8_t *)data_head.data(), data_len + sizeof(t_resp_data));

	// todo crc32 checksum
#ifndef USE_UDP
	write(sock, data_head.data(), data_head.size());
#else
	sendto(sock, data_head.data(), data_head.size(), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
#endif

	return 0;
}




// command = 0x11 QT获取支持分辨率列表
int proc_cmd::proc_get_resolution(unsigned char *data, unsigned int data_len)
{
	unsigned char buff[100];
	int size;
	int ret = 0;

#ifndef X86
	buff[0] = 51; 
	buff[1] = 1;
/*
	ret = hi_get_resolution(&buff[2], &size);
	if(ret != 0)
	{
		proc_err("ERROR to get  resolution:%d\n", ret);
	}
*/
	ret_cmd(0, (char *)buff, size+2);
	return EB_NO_RETURN;
#endif
	return 0;
}

// command = 0x12 QT修改分辨率
int proc_cmd::proc_change_resolution(unsigned char *data, unsigned int data_len)
{
	std::string cmd;
	int w, h;
	unsigned char *resolution_type = data;
	int ret = 0;

	// 检查数据长度
	if(1 + 2 != data_len)
	{
		proc_err("ERROR in data len!\n");
		return EB_WRONG_CONTENT_LEN;
	}

	proc_dbg("will change resolutin type:%d\n", *(unsigned char *)resolution_type);
#ifndef X86

#endif
	return 0;
}

#include "multi_audio_mix.h"
// 0x88
int proc_cmd::proc_change_volume(unsigned char *data, unsigned int data_len)
{
	std::string cmd;
	int w, h;
	int ret = 0;

	// 检查数据长度
	if(5 + 2 != data_len)
	{
		proc_err("ERROR in data len!\n");
		return EB_WRONG_CONTENT_LEN;
	}

	//proc_dbg("will change resolutin type:%d\n", *(unsigned char *)resolution_type);
#ifndef X86

#endif

	struct set_volume *set = (struct set_volume *)data;
	char buf[100] = {0};
	sprintf(buf, "%d.%d", set->f, set->s);	
	printf("type:%d  volume:%s\n", set->type, buf);
	ret = set_mix_volume((unsigned char)set->type, (const char *)buf);

	return 0;
}





int proc_cmd::close()
{
	int ret;
	if(this->sock >= 0)
	{
		ret = shutdown(this->sock, SHUT_WR);
		if(ret != 0)
		{
			proc_err("ERROR to shutdown sock:%d==>%s\n", this->sock, strerror(errno));
		}
	}

	this->sock = -1;
}



// 如果命令解析失败返回错误，并退出之
int proc_cmd::recv_cmd()
{
	int ret;
#ifndef USE_UDP
	std::string data_head;
	data_head.resize(500);
	// 1. 阻塞接收命令,并校验命令
	/* 接收数据头 */
	ret = read(sock, (void *)data_head.data(), sizeof(t_recv_data));
	if(ret != sizeof(t_recv_data))
	{
		proc_err("error to read data head:%d--%s\n", errno, strerror(errno));
		// todo send data
		//ret_cmd(EB_WRONG_HEAD);
		ret_cmd(EB_DATA_SHORT);
		return 0;
	}

	/* 接收数据内容 */
	t_recv_data *head = (t_recv_data *)data_head.data();
	group_id = head->group_id;
	command_id = head->command_id;
	std::string data_content;
	data_content.resize(head->data_len+100);

	uint16_t *crc16 = (uint16_t *)( (char *)(data_head.data() + sizeof(t_recv_data)) + head->data_len - 2);
	uint16_t data_crc16 = crc16_calc( (uint8_t *)(data_head.data() + sizeof(t_recv_data)), head->data_len - 2);
	proc_dbg("head:%02x %02x \n", head->head1, head->head2);
	proc_dbg("group_id = %d\n", group_id);
	proc_dbg("command_id = %d\n", command_id);
	proc_dbg("data len=%d\n", head->data_len);
	proc_dbg("recv crc16 : %04x,  calc crc16:%04x\n", *crc16, data_crc16);

	ret = read(sock, (void *)data_content.data(),  head->data_len);
	if(ret != head->data_len)
	{
		// 数据长度不对
		//printf("read ret=%d, head_len=%d\n", ret, head->data_len);
		proc_err("error to read data content:%d--%s\n", errno, strerror(errno));
		// todo send data
		ret_cmd(EB_DATA_SHORT);
		return 0;
	}
#else
	// udp 处理数据
	std::string data_head;
	data_head.resize(10000);
	cliaddr_len = sizeof(cliaddr);
	ret = recvfrom(sock, (void *)data_head.data(), data_head.size(), 0, (struct sockaddr *)&cliaddr, &cliaddr_len);
	if(ret < 0)
	{
		proc_err("error to recv udp data:%d--%s\n", errno, strerror(errno));
		ret_cmd(EB_NET_RECV_FAIL);
		return 0;
	}

	// 接收完成数据, 处理数据头
	t_recv_data *head = (t_recv_data *)data_head.data();
	group_id = head->group_id;
	command_id = head->command_id;

	uint16_t *crc16 = (uint16_t *)( (char *)(data_head.data() + sizeof(t_recv_data)) + head->data_len - 2);
	uint16_t data_crc16 = crc16_calc( (uint8_t *)(data_head.data()), head->data_len + sizeof(t_recv_data) - 2);
	proc_dbg("head:%02x %02x \n", head->head1, head->head2);
	proc_dbg("group_id = %d\n", group_id);
	proc_dbg("command_id = %02x\n", command_id);
	proc_dbg("data len=%d\n", head->data_len);
	proc_dbg("recv crc16 : %04x,  calc crc16:%04x\n", *crc16, data_crc16);

#if 1
	if(*crc16 != data_crc16)
	{
		proc_err("ERROR in CHECKSUM_FAIL, in data crc16:%04x, here calc crc16:%04x\n",
				*crc16, data_crc16); 
		ret_cmd(EB_CHECKSUM_FAIL);
		return 0;
	}
#endif

	std::string data_content;
	data_content.resize(head->data_len);
	
	memcpy((void *)data_content.data(), (void *)(data_head.data() + sizeof(t_recv_data)), head->data_len);
#endif

#if 1
	// command = 0x11 QT获取支持分辨率列表
	if(command_id == 0x11)
	{
		ret = proc_get_resolution((unsigned char *)data_content.data(), data_content.size());
	}
	else if(command_id == 0x80)
	{
		ret = proc_change_volume((unsigned char *)data_content.data(), data_content.size());
	}
	else
	{
		proc_err("ERROR command id\n");
		ret_cmd(EB_WRONG_HEAD);
		return 0;
	}
	proc_dbg("return value:%d", ret);

	if(ret == 0xffffffff)
	{
		return 0;
	}
	
	if(ret == 0)
	{
		proc_err("will return \n");
		ret_cmd();
		return 0;
	}
	ret_cmd(ret);
#endif

	// 2. 处理命令并返回结果

	return 0;
}

// 检查信号中断线程
void proc_cmd::th_check_connect(void *param)
{

}
