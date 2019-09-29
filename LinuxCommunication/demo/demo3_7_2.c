/*标准输入输出*/
#include <stdio.h>
#include <unistd.h>

int main(int argc, char* const* argv)
{
	printf("start process!\n");
	//STDIN_FILENO,STDERR_FILENO
	write(STDOUT_FILENO, "AAAAAAbb", 6); //标准输出，向屏幕输出

	while (1)
	{
		sleep(1);
		printf("sleep 1 second, pid=%d.\n", getpid());
	}

	printf("exit!\n");
	return 0;
}