/******************************************************************************
 *
 *       Filename:  test_com.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2020年11月21日 17时06分31秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yangkun (yk)
 *          Email:  xyyangkun@163.com
 *        Company:  yangkun.com
 *
 *****************************************************************************/
#include "com.h"

int main()
{
	printf("uart3\n");
	int ret;
	char buff[100];

	CommCreate();

	char send_buff[100] = {"121212121212121212121212121212121212"};

	CommAttribute attr = {
		115200,
		8,
		commNoParity,
		commOneStopBit
	};
	CommOpen();
	CommSetAttribute(&attr);
	printf("size:%d\n", strlen(send_buff));

	while(1)
	{
		//printf("will write uart data\n");
		CommWrite(send_buff, strlen(send_buff));
		ret = CommRead(&buff, 100);
		printf("ret=%d\n", ret);
		if(ret>0)
		{
			printf("recv %d data\n", ret);
			printf("=====> %s\n", buff);
		}
		usleep(2000);
	}
	
}
