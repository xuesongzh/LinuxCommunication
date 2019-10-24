#include <stdio.h>
#include <string.h>

#include "ngx_configer.h"
#include "ngx_defs.h"
#include "ngx_helper.h"

Configer* Configer::mInstance = nullptr;

Configer::Configer()
{

}

Configer::~Configer()
{
	for(auto iter = mConfigInfo.begin(); iter != mConfigInfo.end(); ++iter)
	{
		DEL_PTR(*iter);
	}

	mConfigInfo.clear();
}

Configer::GarCollector::~GarCollector()
{
	DEL_PTR(Configer::mInstance);
}

Configer* Configer::GetInstance()
{
	//锁（boost暂时不引用进来）
	if(mInstance == nullptr)
	{
		mInstance = new Configer();
		static GarCollector garCollector;
	}

	return mInstance;
}

bool Configer::Load(const char* pConfName)
{
	FILE* fp;
	fp = fopen(pConfName, "r");
	if(fp == nullptr)
	{
		return false;
	}

	char linebuf[501]; //每行配置不要太长，保持在500个字符以内，防止出现问题
				
	while(!feof(fp)) //判断文件是否结束
	{
		if(fgets(linebuf, 500, fp) == nullptr) //每次读一行每行不超过500字符
		{
			continue;
		}

		if(linebuf[0] == 0) //空字符
		{
			continue;
		}

		//处理注释行
		if(*linebuf == ';' || *linebuf == ' ' || *linebuf == '[' ||
		*linebuf == '#' || *linebuf == '\t' || *linebuf == '\n')
		{
			continue;
		}

		size_t lineLength = strlen(linebuf);
		//处理以换行，回车，空格结尾的行
		repeat: //需要循环处理保证正确性
		if(lineLength > 0)
		{
			//10:换行,13:回车,32:空格
			if(linebuf[lineLength - 1] == 10 || 
			linebuf[lineLength - 1] == 13 || 
			linebuf[lineLength - 1] == 32)
			{
				linebuf[lineLength - 1] = 0;
				goto repeat;
			}
		}

		//符合配置文件编写标准的行才能走到此处
		char* ptmp = strchr(linebuf, '='); //查找'='位置
		if(ptmp != nullptr)
		{
			PConfItem pConfigItem = new ConfItem();
			memset(pConfigItem, 0, sizeof(ConfItem));
			strncpy(pConfigItem->ItemName, linebuf, static_cast<int>(ptmp - linebuf));
			strcpy(pConfigItem->ItemContent, ptmp + 1);

			//删除字符串首尾空格
			NgxHelper::Ltrim(pConfigItem->ItemName);
			NgxHelper::Rtrim(pConfigItem->ItemName);
			NgxHelper::Ltrim(pConfigItem->ItemContent);
			NgxHelper::Rtrim(pConfigItem->ItemContent);
			
			//以上操作会留下空格，需要截取掉
			mConfigInfo.push_back(pConfigItem);
		}
	}

	fclose(fp); //关闭文件句柄（切记）

	return true;
}

const char* Configer::GetContentByName(const char* pItemName)
{
	for(auto cIter = mConfigInfo.begin(); cIter != mConfigInfo.end(); ++cIter)
	{
		if(strcasecmp((*cIter)->ItemName, pItemName) == 0)
		{
			return (*cIter)->ItemContent;
		}
	}

	return nullptr;
}