#include <stdio.h>
#include "ngx_func.h"

void myconf()
{
	printf("start myconf(), MYVER=%s\n", MYVER);
	return;
}
