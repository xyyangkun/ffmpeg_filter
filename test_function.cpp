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
#include <iostream>
#include <memory>

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


void test_timer()
{
	timer_start();

	while(is_start){
		sleep(1);
	}

	timer_stop();
}


#define TICK_INTERVAL    30
Uint32 TimeLeft(void)
{
	static Uint32 next_time = 0;
	Uint32 now;

	now = SDL_GetTicks();
	printf("    now=%d next_time=%d\n", now, next_time);
	if ( next_time <= now ) {
		//next_time = now+TICK_INTERVAL;
		next_time = next_time + TICK_INTERVAL;
		return(0);
	}
	return (next_time-now);
}



static Uint32 _now = 0;
void TimeLeft0()
{
	_now = SDL_GetTicks();
}

Uint32 TimeLeft1(Uint32 now)
{
	static Uint32 next_time = 0;
	static Uint32 next_time1 = 30;
	//Uint32 now;

	now = SDL_GetTicks();
	printf("    now=%d next_time=%d\n", now, next_time);
	if ( next_time <= now ) {
		// 下一时间应该是在上一次的基础上得到的
		next_time = next_time + TICK_INTERVAL;
		return(0);
	}
	return (next_time-_now);
}

void test_timeleft()
{

	TimeLeft();


	Uint32 left;

	int count = 50;
	//int count = 5000;

	while(count--){
		printf("count:%d time left:%d,   ticks:%d\n", count, left, SDL_GetTicks());
		usleep(10*1000);	

		//SDL_Delay(10);
		left = TimeLeft();
		SDL_Delay(left);
		TimeLeft();
	}
}



int main()
{
	is_start = 1;
	signal(SIGINT, sigterm_handler);


	//test_timer();
	test_timeleft();

	while(is_start){
		sleep(1);
	}

}
