#include <stdio.h>
#include <stdlib.h> //exit
#include <unistd.h> //fork
#include <signal.h>


void sig_usr(int signo)
{
	printf("get SIGUSER1 signal, pid=%d!\n", getpid());
}

int main(int argc, char* const* argv)
{
	pid_t pid;
	printf("start process!\n");
	
	if (signal(SIGUSR1, sig_usr) == SIG_ERR)
	{
		printf("can not catch SIGUSR1 signal!\n");
		exit(1);
	}

	pid = fork(); //创建子进程

	if (pid < 0)
	{
		printf("creat sub process failed!\n");
		exit(1);
	}

	//此时，父进程和子进程一起运行
	while (1)
	{
		sleep(1);
		printf("sleep 1 second, pid=%d!\n", getpid());
	}

	printf("exit!\n");

	return 0;
}