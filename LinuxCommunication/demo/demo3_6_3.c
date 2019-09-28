#include <stdio.h>
#include <unistd.h>

int main(int argc, char* const* argv)
{
	fork(); //假定fork()成功
	fork();

	//((fork() && fork()) || (fork() && fork()));
	//printf("the max number of process=%ld!\n", sysconf(_SC_CHILD_MAX));

	while (1)
	{
		sleep(1);
		printf("sleep 1 second, pid=%d!\n", getpid());
	}

	printf("exit!\n");
	return 0;
}