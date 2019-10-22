
#include <stdio.h>
#include "ngx_configer.h"
#include "ngx_defs.h"

Configer* Configer::mInstance = nullptr;

Configer::Configer()
{
	
}

Configer::~Configer()
{

}

Configer::GarCollector::~GarCollector()
{
	DEL_PTR(Configer::mInstance);
}

Configer* Configer::GetInstance()
{
	//锁（boost暂时不引用进来）
	if(mInstance != nullptr)
	{
		mInstance = new Configer();
		static GarCollector garCollector;
	}

	return mInstance;
}


void Configer::SetConfigInfo(const std::vector<PConfItem>& config)
{
	mConfigInfo = config;
}

std::vector<PConfItem> Configer::GetConfigInfo() const
{
	return mConfigInfo;
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

	while(!feof(fp))
	{
		
	}

	fclose(fp); //关闭文件句柄（切记）

	return true;
}