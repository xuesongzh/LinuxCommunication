#include <string.h>

#include "ser_configer.h"

IMPLEMENT_SINGLETON(SerConfiger)

SerConfiger::~SerConfiger()
{
    
}

bool SerConfiger::Load(const char* const& pConfFileName)
{
    FILE* fp;
    fp = fopen(pConfFileName, "r");
    if (nullptr == fp)
    {
        //日志
        return false;
    }

    char linebuf[501]; //配置文件规定了大小，不会超过500
    while(!feof(fp))
    {
        if (nullptr == fgets(linebuf, 500, fp))
        {
            continue;
        }

        //处理注释行
        if (*linebuf == ';' || *linebuf == ' ' || *linebuf == '#' ||
            *linebuf == '[' || *linebuf == 0 || *linebuf == '\n' || *linebuf == '\t')
        {
            continue;
        }

    lblprocstring: //将配置项末尾的换行，回车，空格去掉
        size_t bufferLength = strlen(linebuf);
        if (bufferLength > 0)
        {
            if (linebuf[bufferLength - 1] == 10 || //换行符
                linebuf[bufferLength - 1] == 13 || //回车
                linebuf[bufferLength - 1] == 32) // 空格
            {
                linebuf[bufferLength - 1] = 0;
                goto lblprocstring;
            }
        }

        //正确的配置项才会走到这里
        printf("%s\n", linebuf);
    }


    fclose(fp);
    return true;
}