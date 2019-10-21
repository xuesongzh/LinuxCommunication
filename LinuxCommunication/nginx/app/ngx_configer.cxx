#include "ngx_configer.h"
#include "ngx_defs.h"

Configer* Configer::mInstance = nullptr;

Configer::GarCollector::~GarCollector()
{
	// DEL_PTR(Configer::mInstance);
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

bool Configer::Load(const char* pConfName)
{

	return true;
}