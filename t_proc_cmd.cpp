/******************************************************************************
 *
 *       Filename:  t_proc_cmd.cpp
 *
 *    Description:  test proc cmd
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
static int rx_loop = 1;
static void _rx_signal(int signo)
{
	signal(SIGINT, SIG_IGN); 
	signal(SIGTERM, SIG_IGN);
	if (SIGINT == signo || SIGTERM == signo)
	{
		rx_loop = 0; 
		exit(0);
	}
}
int main() {
	signal(SIGINT, _rx_signal);
	signal(SIGTERM, _rx_signal);
	// 接收信号10更新zlog配置 kill -10 sigprograme
	//signal(SIGUSR1, _sig);

	proc_cmd cmd;
	cmd.init();

	int count=0; 
	while (rx_loop) {
		cmd.recv_cmd();
		printf("after process %d message\n", count++);
	}

}
