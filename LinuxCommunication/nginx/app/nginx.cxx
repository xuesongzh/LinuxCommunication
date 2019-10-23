#include <stdio.h>
#include <stdlib.h> //exit()

#include "ngx_configer.h"

int main(int argc, char* const* argv)
{
	Configer* pConfiger = Configer::GetInstance();
	if(!pConfiger->Load("nginx.conf"))
	{
		printf("load config failed!\n");
		exit(1);
	}

	// auto conf = pConfiger->GetContentByName("ListenPort");
	// printf("%s\n", conf);

	return 0;
}