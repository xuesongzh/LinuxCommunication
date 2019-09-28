#include <stdio.h>
#include <stdlib.h> //exit
#include <unistd.h> //fork
#include <signal.h>
#include <sys/wait.h> //waitpid


void sig_usr(int signo)
{
	int status;

	switch (signo)
	{
	case SIGUSR1:
		printf("get SIGUSER1 signal, pid=%d!\n", getpid());
		break;
	case SIGCHLD:
		printf("get SIGCHLD signal, pid=%d!", getpid());

		/*
		waitpid:获取子进程的终止状态，这样子进程就不会成为僵尸进程
		-1：等待任何子进程
		&status:保存子进程的状态信息到status
		WHOHANG：提供额外选项，WHOHANG表示不要阻塞，让waitpid立即返回
		*/
		pid_t pid = waitpid(-1, &status, WNOHANG);	

		if (pid == 0)//子进程没有结束，会立即返回0
		{
			return;
		}
		else if (pid == -1)//waitpid调用错误
		{
			return;
		}

		return;
		break;
	default:
		break;
	}
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

	if (signal(SIGCHLD, sig_usr) == SIG_ERR)
	{
		printf("can not catch SIGCHLD signal!\n");
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