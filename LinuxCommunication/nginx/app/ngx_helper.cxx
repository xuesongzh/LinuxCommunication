#include <string.h>
#include "ngx_helper.h"

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