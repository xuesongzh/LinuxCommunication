#include <stdio.h>
// #include <stdlib.h> //exit()
// #include <string.h>
// #include <unistd.h>

// #include "ngx_configer.h"
// #include "ngx_helper.h"
// #include "ngx_log.h"

#include "ser_configer.h"

int main(int argc, char* const* argv)
{

	SerConfiger* pConfiger = SerConfiger::GetInstance();
	printf("%p\n", pConfiger);
	//修改环境变量的位置以修改进程标题
	// g_os_argv = (char**) argv;
	// NgxHelper::NgxInitProcTitle(); //将环境变量搬家

	// //读取配置文件
	// Configer* pConfiger = Configer::GetInstance();
	// if(!pConfiger->Load("nginx.conf"))
	// {
	// 	printf("load config failed!\n");
	// 	exit(1);
	// }

	// //日志初始化
	// ngx_log_init();
	// ngx_log_stderr(0, "fhfu");


	// NgxHelper::NgxSetProcTitle("nginx: master process");


	// DEL_PTR(gp_envmem);//释放分配的新的环境变量内存

	printf("程序退出!\n");

	return 0;
}