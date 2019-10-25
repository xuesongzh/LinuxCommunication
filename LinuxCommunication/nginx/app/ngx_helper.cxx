#include <string.h>
#include <unistd.h> //environ

#include "ngx_helper.h"
#include "ngx_datamodel.h"

char** g_os_argv; //在main中已经赋值
char* gp_envmem = nullptr; //指向新的环境变量地址
int g_environlen = 0; //环境变量内存大小

void NgxHelper::Rtrim(char* string)
{
    size_t len = 0;
    if(string == nullptr)
    {
        return;
    }

    len = strlen(string);
    while(len > 0 && string[len -1] == ' ')
    {
        string[--len] = 0;
    }

    return;
}

void NgxHelper::Ltrim(char* string)
{
    size_t len = 0;
    if(string == nullptr)
    {
        return;
    }

    len = strlen(string);
    char* ptmp = string;
    if( (*ptmp) != ' ') //不是空格开头
    {
        return;
    }

    //找到第一个不是空格的位置
    while( (*ptmp) != '\0')
    {
        if( (*ptmp) == ' ')
        {
            ptmp++;
        }
        else
        {
            break;
        }
    }

    if( (*ptmp) == '\0') //全是空格
    {
        *string = '\0';
        return;
    }

    char* ptmp2 = string;
    while( (*ptmp) != '\0')
    {
        (*ptmp2) = (*ptmp);
        ptmp++;
        ptmp2++;
    }

    (*ptmp2) = '\0';
    return;
}

void NgxHelper::NgxInitProcTitle()
{
    //统计环境变量所占的内存，方法是environ[i]是否为空作为结束标记
    for(int i = 0; environ[i]; ++i)
    {
        g_environlen += strlen(environ[i]) +1; //+1是'\0'
    }

    gp_envmem = new char[g_environlen]; //不需要判空，因为有的编译器就是返回nullptr
    memset(gp_envmem, 0, g_environlen); //清空防止出现问题

    char* ptmp = gp_envmem;

    for(int i = 0; environ[i]; ++i)
    {
        size_t size = strlen(environ[i]) + 1;
        strcpy(ptmp, environ[i]);
        environ[i] = ptmp; //修改环境变量地址
        ptmp += size; 
    }

    return;
}

void NgxHelper::NgxSetProcTitle(const char *ptitle)
{
    //我们假设，所有的命令 行参数我们都不需要用到了，可以被随意覆盖了；
    //我们的标题长度，不会长到原始标题和原始环境变量都装不下，否则怕出问题，不处理

    //计算新标题的长度
    size_t newTitleLenth = strlen(ptitle);

    //计算总的原始argv内存长度
    size_t argvLen = 0;
    for(int i = 0; g_os_argv[i]; ++i)
    {
        argvLen += strlen(g_os_argv[i]) + 1;
    }

    size_t totalLen = g_environlen + argvLen;
    if(totalLen <= newTitleLenth)
    {
        //标题太长，超过了argv和环境变量的总长度
        return;
    } 

    //设置后续的命令行参数为空，表示只有argv[]中只有一个元素了，防止后续argv被滥用，因为很多判断是用argv[] == NULL来做结束标记判断的;
    g_os_argv[1] = NULL;

    //设置标题
    char* ptmp = g_os_argv[0];
    strcpy(ptmp, ptitle);
    ptmp += newTitleLenth;

     //把剩余的原argv以及environ所占的内存全部清0，否则会出现在ps的cmd列可能还会残余一些没有被覆盖的内容；
    size_t len = totalLen - newTitleLenth;  //内存总和减去标题字符串长度(不含字符串末尾的\0)，剩余的大小，就是要memset的；
    memset(ptmp,0,len);

    return;     
}