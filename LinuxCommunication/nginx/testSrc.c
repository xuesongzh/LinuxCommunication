#include <stdio.h>
#include <unistd.h>

int main()
{

	printf("干你Y的\n");

	for(;;)
	{
		sleep(1);
		printf("休息一秒\n");
	}

	printf("程序退出\n");

	return 0;
}
