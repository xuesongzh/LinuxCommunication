#include<stdio.h>
#include<signal.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>


#include "ser_function.h"
#include "ser_configer.h"
#include "ser_log.h"

static void ser_start_worker_processes(int processNum);
static int ser_spawn_process(int processIndex);
static void ser_worker_process_cycle(int processIndex);
static void ser_worker_process_init(int processIndex);

void ser_master_process_cycle()
{
	sigset_t sigSet; //信号集
	sigemptyset(&sigSet); //清空信号集
	//设置阻塞信号，保护代码执行过程中，由信号引起的中断
	sigaddset(&sigSet, SIGCHLD); //子进程状态发生改变
	sigaddset(&sigSet, SIGALRM); //定时器超时
	sigaddset(&sigSet, SIGIO); //异步I/O
	sigaddset(&sigSet, SIGINT); //终端中断
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
	int workerprocesses = SerConfiger::GetInstance()->GetIntDefault("WorkerProcesses", 1);
	ser_start_worker_processes(workerprocesses);

	//创建子进程之后，清空信号屏蔽字
	sigemptyset(&sigSet);

	for(;;)
	{
		sleep(1);
		SER_LOG_STDERR(0, "父进程循环,pid = %p", ser_pid);
		//sigsuspend();
		//a)根据给定的参数设置新的mask 并 阻塞当前进程【因为是个空集，所以不阻塞任何信号】
        //b)此时，一旦收到信号，便恢复原先的信号屏蔽【我们原来的mask在上边设置的，阻塞了多达10个信号，从而保证我下边的执行流程不会再次被其他信号截断】
        //c)调用该信号对应的信号处理函数，之前已经注册过的函数
        //d)信号处理函数返回后，sigsuspend返回，使程序流程继续往下走
		// sigsuspend(&sigSet); //master进程信号驱动
		
	}

	return;
}

static void ser_start_worker_processes(int processNum)
{
	for(int i = 0; i < processNum; ++i)
	{
		if(-1 == ser_spawn_process(i))
		{
			SER_LOG(SER_LOG_ALERT, errno, "create worker process failed! index:%d", processNum);
		}
	}

	return; //父进程返回不在这里循环
}

//fork()子进程
static int ser_spawn_process(int processIndex)
{
	pid_t pid;
	pid = fork();
	switch(pid)
	{
	case -1: //创建进程失败
		SER_LOG(SER_LOG_ALERT, errno, "create worker process failed!");
		return -1;
	case 0: //子进程
		ser_parent_pid = ser_pid;
		ser_pid = getpid();
		ser_worker_process_cycle(processIndex);
		break;
	default: //父进程，返回的是子进程的ID
		break;
	}

	return pid;
}

//workr子进程循环，干活
static void ser_worker_process_cycle(int processIndex)
{
	ser_worker_process_init(processIndex);
	//设置子进程标题
	const char* pWorkerProcessTitle = SerConfiger::GetInstance()->GetString("WorkerProcessTitle");
	SetProcessTitle(pWorkerProcessTitle);

	//子进程循环体
	for(;;)
	{

		sleep(1);
		SER_LOG_STDERR(0, "子进程循环,pid = %p", ser_pid);
	}

	return;
}

//子进程创建时的初始化工作
static void ser_worker_process_init(int processIndex)
{
	sigset_t sigSet;
	sigemptyset(&sigSet); //原来有信号屏蔽字，现在清空
	
	if(sigprocmask(SIG_SETMASK, &sigSet, NULL) == -1)
	{
		SER_LOG(SER_LOG_ALERT, errno, "worker process init: sigprocmask failed!");
	}

	//以后扩充代码....
	return;
}