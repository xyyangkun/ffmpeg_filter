/******************************************************************************
 *
 *       Filename:  test_sdl_timer.c
 *
 *    Description:  测试sdl timer 定时器使用
 *    https://docs.huihoo.com/game/sdl/1.0-intro.cn/usingtimers.html
 *    https://wiki.libsdl.org/SDL_GetTicks
 *    https://blog.csdn.net/qq_40953281/article/details/80214908
 *    https://www.cnblogs.com/landmark/archive/2012/05/25/2517434.html
 *
 *
 *        Version:  1.0
 *        Created:  2021年12月13日 18时36分28秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
/*!
* \file test_sdl_timer.c
* \brief 测试sdl timer定时器使用
* 
* 
* \author yangkun
* \version 1.0
* \date 2021.12.13
*/

#include "SDL2/SDL.h"

#include <unistd.h>

#define TICK_INTERVAL    30

#define TICK_INTERVAL1    10


Uint32 TimeLeft(void)
{
	static Uint32 next_time = 0;
	Uint32 now;

	now = SDL_GetTicks();
	if ( next_time <= now ) {
		next_time = now+TICK_INTERVAL;
		return(0);
	}
	return(next_time-now);
}

int test_timeleft()
{

	TimeLeft();


	Uint32 left;

	int count = 10;
	while(count--){
		usleep(10*1000);	
		left = TimeLeft();
		SDL_Delay(left);
		printf("time left:%d\n", left);
	}

}

static int count = 0;
// 返回值是下次回调的间隔时间，0出错了
// 回调不关心你调用了多少次
Uint32 callback(Uint32 interval, void *param)//回调函数 
{
	count++;
	printf("=============> count:%d\n", count);
	usleep(30);

	return TICK_INTERVAL;
}

static int count1 = 0;
Uint32 callback1(Uint32 interval, void *param)//回调函数 
{
	count1++;
	printf("=============> count1:%d\n", count1);
	usleep(3);

	return TICK_INTERVAL1;
}


// 测试定时器回调
// 添加两路定时器

int main() 
{
	//Uint32 delay = (33 / 10) * 10; 
	SDL_Init(SDL_INIT_TIMER);

	test_timeleft();
	printf("test_timeleft:%d\n", 0);


	//
	SDL_TimerID timer = SDL_AddTimer(TICK_INTERVAL, callback, NULL);//创立定时器timer 
	SDL_TimerID timer1 = SDL_AddTimer(TICK_INTERVAL1, callback1, NULL);//创立定时器timer 

	sleep(2);
	SDL_RemoveTimer(timer);
	SDL_RemoveTimer(timer1);
	printf("count=%d\n", count);

	printf("will over\n");
}

