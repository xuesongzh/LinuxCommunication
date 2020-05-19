#include<stdio.h>
#include<signal.h>
#include<unistd.h>
#include<string.h>


#include "ser_function.h"
#include "ser_configer.h"
#include "ser_log.h"



void ser_master_process_cycle()
{
	sigset_t sigSet; //信号集
	sigemptyset(&sigSet); //清空信号集
	//设置阻塞信号，保护代码执行过程中，由信号引起的中断
	sigaddset(&sigSet, SIGCHLD); //子进程状态发生改变
	sigaddset(&sigSet, SIGALRM); //定时器超时
	sigaddset(&sigSet, SIGIO); //异步I/O
	sigaddset(&sigSet, SIGINT); //终端终端
	sigaddset(&sigSet, SIGHUP); //连接断开
	sigaddset(&sigSet, SIGUSR1); //用户信号
	sigaddset(&sigSet, SIGUSR2); //用户信号
	sigaddset(&sigSet, SIGWINCH); //终端窗口大小改变
	sigaddset(&sigSet, SIGTERM); //终止
	sigaddset(&sigSet, SIGQUIT); //终端退出
	//以后根据开发需要增加......

	if(-1 == sigprocmask(SIG_BLOCK, &sigSet, NULL))
	{
		SER_LOG(SER_LOG_ALERT, errno, "ser_master_process_cycle:sigprocmask failed!");
		//这里不处理，让流程继续往下执行
	}
	
    //设置主进程标题
	//这里不用判断SerConfiger::GetInstance()是否为空，如果为空在main()中已经返回
	const char* pMasterProcessTitle = SerConfiger::GetInstance()->GetString("MasterProcessTitle");
	if (nullptr == pMasterProcessTitle)
	{
		SER_LOG(SER_LOG_ALERT ,errno, "get master process title failed!");
		//这里不处理，让流程继续往下执行
	}

	size_t titleLength = 0; //名字+命令行参数的长度
	titleLength = sizeof(pMasterProcessTitle);
	titleLength += ArgvLength + (ArgcNumber - 1)/*命令行参数之间空格的个数*/;
	if(titleLength < 1000) //小于1000才设置标题
	{
		char title[1000] = {0};
		strcpy(title, pMasterProcessTitle);
		strcat(title, " "); //追加空格
		for(int i = 0; i < ArgcNumber; ++i)
		{
			strcat(title, pArgv[i]);
			if(i != ArgcNumber -1)
			{
				strcat(title, " ");//如果不是最后一个参数，追加空格
			}
		}
		//设置进程标题：如Server ./server -a -b -c	
		SetProcessTitle(title);
	}

	//创建子进程



	//创建子进程之后，清空信号屏蔽字
	sigemptyset(&sigSet);

	while(true)
	{
		// SER_LOG_STDERR(0, "父进程循环,pid = %p", ser_pid);
	}

	return;
}