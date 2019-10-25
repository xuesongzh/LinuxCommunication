#include <stdio.h>
#include <stdlib.h> //exit()
#include <string.h>
#include <unistd.h>

#include "ngx_configer.h"
#include "ngx_helper.h"

int main(int argc, char* const* argv)
{
	//修改环境变量的位置以修改进程标题
	g_os_argv = (char**) argv;
	NgxHelper::NgxInitProcTitle(); //将环境变量搬家

	//读取配置文件
	Configer* pConfiger = Configer::GetInstance();
	if(!pConfiger->Load("nginx.conf"))
	{
		printf("load config failed!\n");
		exit(1);
	}

	NgxHelper::NgxSetProcTitle("nginx: master process");
	
	while(1)
	{

	}

	return 0;
}