#include <stdio.h>
#include <unistd.h>

#include "ngx_func.h"
#include "ngx_signal.h"

int main(int argc, char* const* argv)
{
	printf("start main()\n");
	myconf();
	mysignal();

	printf("exit!\n");
	return 0;
}