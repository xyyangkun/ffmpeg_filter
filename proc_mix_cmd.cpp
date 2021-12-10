/******************************************************************************
 *
 *       Filename:  t_proc_cmd.cpp
 *
 *    Description:  接收命令控制音量
 *
 *        Version:  1.0
 *        Created:  2021年11月29日 20时38分09秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
#include <iostream>
#include "proc_cmd.h"
#include "multi_audio_mix.h"
static int rx_loop = 1;
static void _rx_signal(int signo)
{
	signal(SIGINT, SIG_IGN); 
	signal(SIGTERM, SIG_IGN);
	if (SIGINT == signo || SIGTERM == signo)
	{
		set_mix_exit();
		rx_loop = 0; 
		exit(0);
	}
}


pthread_t t_mix;

//  创建线程
int init_mix(){
	int ret;
	t_mix = 0;
	ret = pthread_create(&t_mix, NULL, multi_audio_mix_proc, NULL);
	if(ret != 0){
		printf("error to create thread:errno = %d\n", errno);
		assert(1);
	}

	return 0;
}

int deinit_mix(){
	if(t_mix!=0) {
		pthread_join(t_mix, NULL);
		t_mix = 0;
	}
	return 0;
}

int main() {
	signal(SIGINT, _rx_signal);
	signal(SIGTERM, _rx_signal);
	// 接收信号10更新zlog配置 kill -10 sigprograme
	//signal(SIGUSR1, _sig);

	proc_cmd cmd;
	cmd.init();

	init_mix();

	int count=0; 
	while (rx_loop) {
		cmd.recv_cmd();
		printf("after process %d message\n", count++);
	}


	deinit_mix();
}
