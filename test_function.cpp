/******************************************************************************
 *
 *       Filename:  test_function.c
 *
 *    Description:  测试函数 
 *    测试sdl timer 在系统时间向前更改后，定时器暂停的问题
 *
 *        Version:  1.0
 *        Created:  2022年05月31日 15时07分24秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/

#include "SDL2/SDL.h"
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

#define AUIDIO_TICK_INTERVAL    20
static SDL_TimerID timer ;

static int is_start = 0;

static void sigterm_handler(int sig) 
{
	printf("get sig:%d\n", sig);
	is_start = 0;
}

static unsigned int audio_not_exist_cb(unsigned int p1, void *p)
{
	printf("========================> debug!!\n");
	return AUIDIO_TICK_INTERVAL;
}


/*
 * 定时器 验证更改系统实现对定时器的影响
 */
int timer_start()
{
	SDL_Init(SDL_INIT_TIMER);
	timer = SDL_AddTimer(AUIDIO_TICK_INTERVAL, audio_not_exist_cb, NULL);//创立定时器timer 

	return 0;
}

int timer_stop()
{
	// 删除sdl定时器
	SDL_RemoveTimer(timer);
	return 0;
}


int main()
{
	is_start = 1;
	signal(SIGINT, sigterm_handler);

	timer_start();

	while(is_start){
		sleep(1);
	}

	timer_stop();
}
