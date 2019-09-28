#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h> //exit()

//信号处理函数
void sig_quit(int signo)
{
	printf("get QUIT signal.\n");
	if (signal(SIGQUIT, SIG_DFL) == SIG_ERR)//设置SIGQUIT信号默认操作
	{
		printf("can not do default operation and quit process.\n");
		exit(1);
	}
}


int main(int argc, char* const* argv)
{
	sigset_t newmask, oldmask;//信号集，新的信号集与旧的信号集<signal.h>
	if (signal(SIGQUIT, sig_quit) == SIG_ERR)
	{
		printf("can not catch SIGQUIT signal.\n");
		exit(1);//0表示正常退出
	}

	sigemptyset(&newmask);//将newmask信号集清零
	sigaddset(&newmask, SIGQUIT); //设置newmask中的SIGQUIT信号为为1，表示阻塞当前信号

	//sigprocmask();设置该进程对应的信号集
	/*
	SIG_BLOCK:设置新的信号集为当前信号集和第二个参数指向的信号集的并集
	&newmask: 和当前信号集作并集处理
	&oldmask: 第三个参数不为空表示将调用sigprocmask()之前的旧的信号集保存在该变量中，可以用于后续恢复
	*/
	if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0)
	{
		printf("sigprocmask(SIG_BLOCK) failed.\n");
		exit(1);
	}

	printf("---begin---sleep 10 seconds, and can not accept SIGQUIT signal.\n");
	sleep(10); //此时无法接受SIGQUIT信号
	printf("---end---sleep 10 seconds.\n");

	if (sigismember(&newmask, SIGQUIT))//测试newmask中SIGQUIT屏蔽字是否为1
	{
		printf("can not accept SIGQUIT signal.\n");
	}
	else
	{
		printf("can accept SIGQUIT sgnal.\n");
	}

	if (sigismember(&newmask, SIGHUP))
	{
		printf("can not accept SIGHUP signal.\n");
	}
	else
	{
		printf("can accept SIGHUP sgnal.\n");
	}

	//将信号集还原为oldmask
	/*
	SIG_SETMASK:设置进程新的信号集为第二个参数指向的信号集，第三个参数没用
	*/
	if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0)
	{
		printf("sigprocmask(SIG_SETMASK) fsiled.\n");
		exit(1);
	}

	printf("sigprocmask(SIG_SETMASK) sucess.\n");

	if (sigismember(&oldmask, SIGQUIT))
	{
		printf("can not accept SIGQUIT signal.\n");
	}
	else
	{
		printf("can accept SIGQUIT signal and i will sleep 20 seconds.\n");
		int mysl = sleep(20);
		if (mysl > 0)
		{
			printf("has %d seconds left.\n", mysl);
		}
	}

	printf("exit.\n");
	return 0;
}