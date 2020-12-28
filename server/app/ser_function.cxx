#include "ser_function.h"

#include <signal.h>
#include <string.h>
#include <unistd.h>  //environ

#include "ser_datastruct.h"

#pragma region[字符串相关]

//截取字符串尾部空格
void Rtrim(char* string) {
    size_t len = 0;
    if (string == NULL)
        return;

    len = strlen(string);
    while (len > 0 && string[len - 1] == ' ')  //位置换一下
        string[--len] = 0;
    return;
}

//截取字符串首部空格
void Ltrim(char* string) {
    size_t len = 0;
    len = strlen(string);
    char* p_tmp = string;
    if ((*p_tmp) != ' ')  //不是以空格开头
        return;
    //找第一个不为空格的
    while ((*p_tmp) != '\0') {
        if ((*p_tmp) == ' ')
            p_tmp++;
        else
            break;
    }
    if ((*p_tmp) == '\0')  //全是空格
    {
        *string = '\0';
        return;
    }
    char* p_tmp2 = string;
    while ((*p_tmp) != '\0') {
        (*p_tmp2) = (*p_tmp);
        p_tmp++;
        p_tmp2++;
    }
    (*p_tmp2) = '\0';
    return;
}

#pragma endregion

#pragma region[设置进程名称]

void MoveEnviron(char* pNewEnviron) {
    char* pTemp = pNewEnviron;
    for (int i = 0; environ[i]; ++i) {
        size_t sizeEnv = strlen(environ[i]) + 1;  //注意'\0'
        strcpy(pTemp, environ[i]);                //复制内容到新内存
        environ[i] = pTemp;                       //改变环境变量指向
        pTemp += sizeEnv;
    }
}

void SetProcessTitle(const char* pTitle) {
    size_t sizeTitle = strlen(pTitle);
    size_t garvLength = 0;
    for (int i = 0; pArgv[i]; ++i) {
        garvLength += strlen(pArgv[i]) + 1;
    }
    //标题名字太长不支持
    size_t totalLength = garvLength + EnvironLength;
    if (sizeTitle > totalLength) {
        return;
    }

    pArgv[1] = NULL;  //for循环中用于判断pArgv[i]
    char* pTemp = pArgv[0];
    strcpy(pTemp, pTitle);
    pTemp += sizeTitle;
    //把标题之后的内存清空
    memset(pTemp, 0, totalLength - sizeTitle);
}

#pragma endregion