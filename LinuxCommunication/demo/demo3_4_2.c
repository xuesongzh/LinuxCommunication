/*不可重入函数错误示例*/
#include<stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h> //malloc


void SigUsr(int signo)
{
	//malloc不可重入函数不能用在信号处理函数中
	int* p = (int*)malloc(sizeof(int));
	free(p);
	//如果不断往该进程发送SIGUSR1或者SIGUSR2信号，将收不到这些信号，也就是以下代码不能执行

	if (signo == SIGUSR1)
	{
		printf("get SIGUSR1 signal.\n");
	}
	else if (signo == SIGUSR2)
	{
		printf("get SIGUSR2 signal.\n");
	}
	else
	{
		printf("get signal that not want.\n");
	}

}



int main(int argc, char* const* argv)
{
	if (signal(SIGUSR1, SigUsr) == SIG_ERR)
	{
		printf("can not catch SIGUSR1 signal.\n");
	}

	if (signal(SIGUSR2, SigUsr) == SIG_ERR)
	{
		printf("can not catch SIGUSR2 signal.\n");
	}

	while (1)
	{
		int* p = (int*)malloc(sizeof(int));
		free(p);
	}

	printf("exit!\n");
	return 0;
}