#include <stdio.h>
#include <unistd.h>

#include<signal.h>

int main()
{

	pid_t pid;

	printf("干你Y的\n");

	// //系统函数，设置某个信号来的时候处理，SIG_IGN忽略信号标志，请操作系统不要用缺省的信号处理方式来处理
	// signal(SIGHUP, SIG_IGN);

	pid = fork(); //系统函数，创建新进程。后面的代码子进程和父进程都会往后执行
	if(pid < 0)
	{
		printf("failed to fork son process.\n");
	}
	else if(pid == 0)//子进程
	{
		printf("son process......\n");
		setsid(); //子进程创建session有效

		for (;;)
		{
			sleep(1);
			printf("子进程休息一秒\n");
		}

		return 0;
	}
	else
	{
		for (;;)
		{
			sleep(1);
			printf("父进程休息一秒\n");
		}
	}

	// setsid();//新建一个session,但是进程组组长调用无效


	// for(;;)
	// {
	// 	sleep(1);
	// 	printf("休息一秒\n");
	// }

	printf("程序退出\n");

	return 0;
}
