#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>


int main(int argc, char* const* argv)
{
	pid_t pid;
	printf("start process!\n");

	//SIG_IGN:忽略标志，请求操作系统不要使用缺省的信号处理方式来处理
	//signal(SIGHUP, SIG_IGN);

	pid = fork();

	if (pid < 0)
	{
		printf("create sub process failed!\n");
		exit(1);
	}
	else if (pid == 0)
	{
		printf("sub process......\n");

		setsid(); //子进程创建session有效
		while (1)
		{
			sleep(1);
			printf("sub process sleep 1 second!\n");
		}

		return 0;
	}
	else
	{
		while (1)
		{
			sleep(1);
			printf("father process slepp 1 second!\n");
		}
	}

	//新建一个session,使其脱离终端的控制，但是进程组组长调用无效
	//setsid();
	//while (1)
	//{
	//	sleep(1);
	//	printf("sleep 1 second!\n");
	//}

	return 0;
}