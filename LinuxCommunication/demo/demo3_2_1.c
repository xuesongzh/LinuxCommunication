#include <stdio.h>
#include <unistd.h> //sleep需要包含的头文件

int main()
{
	printf("start to learn nginx at Linux!\n");

	while (1)
	{
		sleep(1);
		printf("sleep 1 second.\n");
	}

	printf("Exit.\n");

	return 0;
}