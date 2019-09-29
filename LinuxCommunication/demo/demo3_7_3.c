#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <fcntl.h> //O_RDWR
#include <sys/stat.h> //umask

//创建守护进程，成功返回1，否则返回1
int ngx_daemon()
{
	int fd;

	switch (fork())
	{
	case -1:
		return -1;
	case 0: //子进程
		break;
	default: //父进程直接退出
		exit(0);
	}

	//只有子进程会走到这里
	if (setsid() == -1)//脱离终端
	{
		//打印日志.........
		return -1;
	}
	umask(0); //设置为0，不要让他来限制文件权限以免引起混乱

	fd = open("/dev/null", O_RDWR); //打开空设备

	if (dup2(fd, STDIN_FILENO) == -1)//先关闭STDIN_FILENO[这是规矩，已经打开的描述符，动他之前，先close]，类似于指针指向null，让/dev/null成为标准输入；
	{
		//记录日志......
		return -1;
	}

	if (dup2(fd, STDOUT_FILENO) == -1) //先关闭STDOUT_FILENO，类似于指针指向null，让 / dev / null成为标准输出；
	{
		//记录日志.....
		return -1;
	}

	if (fd > STDERR_FILENO) //fd应该是3，应该成立
	{
		if (close(fd) == -1)//释放资源，这样这个文件描述符就可以被复用；不然这个数字【文件描述符】会被一直占着；
		{
			//记录日志......
			return -1;
		}
	}

	return 1;
}

int main(int argc, char* const* argv)
{
	if (ngx_daemon() == -1)
	{
		//记录日志
		return -1;
	}
	else
	{
		while (1)
		{
			sleep(1);
			printf("sleep 1 second!, pid=%d\n", getpid());//打印没用，现在标准输出指向/dev/null
		}
	}

	return 0;
}