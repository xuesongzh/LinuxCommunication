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

int main(int argc, char* const* argv)
{
	//将环境变量搬走
	pArgv = const_cast<char**>(argv);
	for (int i = 0; environ[i]; ++i)
	{
		EnvironLength += strlen(environ[i]) + 1; //环境变量长度，+1是因为'\0'的存在
	}
	pNewEnviron = new char[EnvironLength];
	memset(pNewEnviron, 0, EnvironLength);
	MoveEnviron(pNewEnviron);
	//配置文件加载
	SerConfiger* pConfiger = SerConfiger::GetInstance();
	if (nullptr == pConfiger)
	{
		SER_LOG_STDERR(errno, "create configer failed!");
		exit(1);
	}
	if (true != pConfiger->Load("server.conf"))
	{
		SER_LOG_STDERR(errno, "load configer content failed!");
		exit(1);
	}

	ser_log_init();

	//设置主进程标题
	const char* masterProcessTitle = pConfiger->GetString("MasterProcessTitle");
	if (nullptr == masterProcessTitle)
	{
		SER_LOG_STDERR(errno, "get master process title failed!");
		exit(1);
	}
	SetProcessTitle(masterProcessTitle);

	// while(true)
	// {
	// 	sleep(1);
	// 	printf("sleep 1 second!\n");
	// }

	printf("程序退出!\n");

	//程序退出的时候要释放内存
	DEL_ARRAY(pNewEnviron);

	return 0;
}