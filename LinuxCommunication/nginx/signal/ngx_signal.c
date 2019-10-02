#include <stdio.h>
#include <unistd.h>

void mysignal()
{
	printf("start mgsignal()!\n");
	return;
}