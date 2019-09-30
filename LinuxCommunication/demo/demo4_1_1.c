/*用户态内核态切换，以及信号阻塞*/
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>

void sig_usr(int signo)
{
	if (signo == SIGUSR1)
	{
		printf("get SIGUSR1 signal, sleep 10 seconds---begin---\n");
		sleep(10);
		printf("get SIGUSR1 signal, sleep 10 seconds---end---\n");
	}
	else if (signo == SIGUSR2)
	{
		printf("get SIGUSR2 signal, sleep 10 seconds---begin---\n");
		sleep(10);
		printf("get SIGUSR2 signal, sleep 10 seconds---end---\n");
	}
	else
	{
		printf("can not catch want siganl!\n");
	}
}

int main(int argc, char* const* argv)
{
	if (signal(SIGUSR1, sig_usr) == SIG_ERR)
	{
		printf("can catch SIGUSR1 signal!\n");
	}

	if (signal(SIGUSR2, sig_usr) == SIG_ERR)
	{
		printf("can catch SIGUSR2 signal!\n");
	}

	while (1)
	{
		sleep(1);
		printf("sleep 1 second!\n");
	}

	printf("exit!\n");

	return 0;
}