#include <stdio.h>
#include <stdlib.h> //exit()
#include <unistd.h> //sleep()
#include <string.h>

#include "ser_configer.h"
#include "ser_function.h"
#include "ser_macros.h"
#include "ser_log.h"

//设置进程标题相关全局变量
char** pArgv = nullptr;
char* pNewEnviron = nullptr;
int EnvironLength = 0;
pid_t ser_pid;

static void FreeSource();

int main(int argc, char* const* argv)
{
	int exitCode = 0;
	ser_pid = getpid(); //获取进程ID
	pArgv = const_cast<char**>(argv);
	const char* masterProcessTitle = nullptr; //主进程标题

	//配置文件加载
	SerConfiger* pConfiger = SerConfiger::GetInstance();
	if (nullptr == pConfiger)
	{
		SER_LOG_STDERR(errno, "create configer failed!");
		exitCode = 1;
		goto lblexit;
	}
	if (true != pConfiger->Load("server.conf"))
	{
		SER_LOG_STDERR(errno, "load configer content failed!");
		exitCode = 2; //找不到文件
		goto lblexit;
	}

	//日志系统初始化
	SER_LOG_INIT();

	//将环境变量搬走：为了设置进程标题
	for (int i = 0; environ[i]; ++i)
	{
		EnvironLength += strlen(environ[i]) + 1; //环境变量长度，+1是因为'\0'的存在
	}
	pNewEnviron = new char[EnvironLength];
	memset(pNewEnviron, 0, EnvironLength);
	MoveEnviron(pNewEnviron);
	//设置主进程标题
	masterProcessTitle = pConfiger->GetString("MasterProcessTitle");
	if (nullptr == masterProcessTitle)
	{
		SER_LOG_STDERR(errno, "get master process title failed!");
		exitCode = 1;
		goto lblexit;
	}
	SetProcessTitle(masterProcessTitle);

	// while(true)
	// {
	// 	sleep(1);
	// 	printf("sleep 1 second!\n");
	// }

lblexit:
	FreeSource();
	printf("程序退出!\n");
	return exitCode;
}

static void FreeSource()
{
	//释放新的环境变量所在内存
	DEL_ARRAY(pNewEnviron);	

	//关闭日志文件
	if(ser_log.mFd != STDERR_FILENO && ser_log.mFd != -1)
	{
		close(ser_log.mFd);
		ser_log.mFd = -1; //标记下，防止再次被close
	}
}