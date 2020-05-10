#include <string.h>
#include <strings.h>

#include "ser_configer.h"
#include "ser_function.h"
#include "ser_macros.h"

IMPLEMENT_SINGLETON(SerConfiger)

SerConfiger::~SerConfiger()
{
    for (auto& item : mConfItemList)
    {
        DEL_PTR(item);
    }

    mConfItemList.clear();
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

        if (strlen(linebuf) <= 0)
        {
            continue;
        }

    lblprocstring: //将配置项末尾的换行，回车，空格去掉
        size_t bufferLength = strlen(linebuf);
        if (linebuf[bufferLength - 1] == 10 || //换行符
            linebuf[bufferLength - 1] == 13 || //回车
            linebuf[bufferLength - 1] == 32)   // 空格
        {
            linebuf[bufferLength - 1] = 0;
            goto lblprocstring;
        }

        //处理注释行等
        if (*linebuf == ';' || *linebuf == ' ' || *linebuf == 0 || 
            *linebuf == '\n' || *linebuf == '\t' || *linebuf == 0 ||
            *linebuf == '[' || *linebuf == '#')
        {
            continue;
        }

        //正确的配置项才会走到这里
        char* pTemp = strchr(linebuf, '='); //配置项规则
        if (nullptr != pTemp)
        {
            LPConfItem pConfItem = new ConfItem();
            memset(pConfItem, 0, sizeof(ConfItem));
            strncpy(pConfItem->mName, linebuf,(int)(pTemp - linebuf));
            strcpy(pConfItem->mContent, pTemp+1); //复制到碰到'\0'为止
            
            //截取左右空格
            Rtrim(pConfItem->mContent);
            Rtrim(pConfItem->mName);
            Ltrim(pConfItem->mContent);
            Ltrim(pConfItem->mName);

            mConfItemList.push_back(pConfItem);
        }
    }


    fclose(fp);
    return true;
}


const char* SerConfiger::GetString(const char* const& pItemName)
{
    for (auto& item : mConfItemList)
    {
        if (strcasecmp(item->mName, pItemName) == 0) //忽略大小比较字符串
        {
            return  item->mContent;
        }
    }

    return nullptr;
}

int SerConfiger::GetIntDefault(const char* const& pItemName, int def)
{
    for (auto& item : mConfItemList)
    {
        if (strcasecmp(item->mName, pItemName) == 0)
        {
            return  atoi(item->mContent);
        }
    }

    return def;
}