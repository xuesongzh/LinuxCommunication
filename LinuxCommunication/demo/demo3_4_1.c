/*信号处理函数*/
#include<stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

int g_mysign = 0;

void ModifyFunc(int value)//不可重入函数
{
	g_mysign = value;
}


//信号处理函数,应该尽量简单且避免调用不可重入函数，比如malloc，printf
void SigUsr(int signo)
{
	//如果我们一定要在信号处理函数中调用不可重入函数，需要对可能改变的值进行备份
	int temp_sign = g_mysign;

	ModifyFunc(22);//坚决不要在系统信号处理函数中调用不可重入函数

	int myErrno = errno; //备份系统全局变量

	if (signo == SIGUSR1)
	{
		printf("get SIGUSR1 signal.\n");//不可重入函数，为了测试信号处理函数
	}
	else if (signo == SIGUSR2)
	{
		printf("get SIGUSR2 signal.\n");
	}
	else
	{
		printf("get signal that not want.\n");
	}

	g_mysign = temp_sign;
	errno = myErrno;
}



int main(int argc, char* const* argv)
{
	//signal(信号，函数指针),但是因为可靠性等问题一般不会使用signal
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
		sleep(1);
		printf("sleep 1 second.\n");

		ModifyFunc(15);
		sleep(1);//在此期间调用信号处理函数，会修改g_mysign的值，导致错误
		printf("g_mysign=%d\n", g_mysign);
	}

	printf("exit!\n");
	return 0;
}