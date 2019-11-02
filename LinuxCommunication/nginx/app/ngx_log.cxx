#include <stdio.h>
#include <stdlib.h>
#include <string.h>//memcpy,memset...
#include <fcntl.h> //open
#include <unistd.h> //STDERR_FILENO等
#include <stdlib.h> 
#include <errno.h> //errno
#include <stdarg.h> //var_start...


#include "ngx_log.h"
#include "ngx_configer.h"
#include "ngx_defs.h"
#include "ngx_datamodel.h"

ngx_log_t ngx_log;

void ngx_log_stderr(int err, const char* fmt, ...)
{
    va_list args;
    u_char errstr[NGX_MAX_ERROR_STR + 1]; //+1:'\0'
    u_char* p;
    u_char* last;
    memset(errstr, 0, sizeof(errstr));

    last = errstr + NGX_MAX_ERROR_STR; //用于防止输出字符超过此长度

    p = NGX_CPYMEM(errstr, "nginx: ", 7); //此时p指向字符串"nginx: "的后面

    va_start(args, fmt); //args指向起始参数




}


//日志初始化，将日志文件打开
void ngx_log_init()
{
    u_char* pLogName = nullptr;

    //从配置文件中读取和日志相关的配置信息
    Configer* pConfiger = Configer::GetInstance();
    pLogName = (u_char*)pConfiger->GetContentByName("LogPath");
    if(pLogName == nullptr)
    {
        //没有读取成功，需要设置缺省的路径
        pLogName = (u_char*) NGX_LOG_PATH;
    }

    ngx_log.log_level = pConfiger->GetNumberByName("LogLevel", NGX_LOG_NOTICE); //默认等级6

    //只写打开|追加到末尾|文件不存在则创建【这个需要跟第三参数指定文件访问权限】
    //mode = 0644：文件访问权限，6: 110, 4: 100：【用户：读写， 用户所在组：读，其他：读】
    ngx_log.fd = open((const char*) pLogName, O_WRONLY|O_APPEND|O_CREAT, 0644);

    if(ngx_log.fd == -1) 
    {
        //如果有错误，就直接定位到标准错误上去
        ngx_log_stderr(errno, "[alert] could not open error log file: open() \"%s\" failed", pLogName);
        ngx_log.fd = STDERR_FILENO;

    }
    
}