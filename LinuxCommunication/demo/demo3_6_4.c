#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int g_mygbltest = 0;
int main(int argc, char* const* argv)
{
	pid_t pid;
	printf("start process!\n");

	pid = fork();

	if (pid < 0)
	{
		printf("creat sub process failed!\n");
		exit(1);
	}

	//求证进程写时复制
	if (pid == 0) //子进程
	{
		while (1)
		{
			g_mygbltest++;
			sleep(1);
			printf("sub process pid=%d, g_mygbltest=%d\n", getpid(),g_mygbltest);
		}
	}
	else //父进程
	{
		while (1)
		{
			g_mygbltest++;
			sleep(5);
			printf("father process pid=%d, g_mygbltest=%d\n", getpid(), g_mygbltest);
		}
	}

	return 0;
}