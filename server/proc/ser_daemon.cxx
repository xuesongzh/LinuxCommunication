#include <errno.h>
#include <fcntl.h>  //open
#include <signal.h>
#include <sys/stat.h>  //umask
#include <unistd.h>

#include "ser_datastruct.h"
#include "ser_function.h"
#include "ser_log.h"
#include "ser_macros.h"

//创建守护进程
//失败：-1，子进程：0，父进程：1
int ser_daemon() {
    switch (fork()) {
        case -1:
            SER_LOG(SER_LOG_EMERG, errno, "create daemon process failed!");
            return -1;
        case 0:
            break;
        default:
            return 1;
    }

    //只有子进程才能走到这里
    ser_parent_pid = ser_pid;
    ser_pid = getpid();

    //脱离终端
    if (setsid() == -1) {
        SER_LOG(SER_LOG_EMERG, errno, "setsid failed!");
        return -1;
    }

    //umask(0):不限制文件权限，以免引起混乱
    umask(0);

    //打开空设备，使标准输出和输入指向空设备，不像屏幕输出任何东西
    int fd = open("/dev/null", O_RDWR);  //读写方式打开
    if (-1 == fd) {
        SER_LOG(SER_LOG_EMERG, errno, "open \"/dev/null\"failed!");
        return -1;
    }

    //标准输入指向空设备
    if (dup2(fd, STDIN_FILENO) == -1) {
        SER_LOG(SER_LOG_EMERG, errno, "dup2(fd, STDIN_FILENO) failed!");
        return -1;
    }

    //标准输出指向空设备
    if (dup2(fd, STDOUT_FILENO) == -1) {
        SER_LOG(SER_LOG_EMERG, errno, "dup2(fd, STDIN_FILENO) failed!");
        return -1;
    }

    if (fd > STDERR_FILENO) {
        //释放资源
        if (close(fd) == -1) {
            SER_LOG(SER_LOG_EMERG, errno, "close(fd) failed!");
            return -1;
        }
    }

    return 0;
}