/*
==============================
*Copyright(c): huan.liu, 2019
*All rights reversed

*File name: ngx_configer.h
*Brief: class Configer 用于读取配置文件, 单例类

*Data:2019.10.16
*Version: 1.0
==============================
*/

#ifndef __NGX_CONFIGER_H__
#define __NGX_CONFIGER_H__

#include <vector>
#include "ngx_datamodel.h"

class Configer
{
  private:
	Configer();

  public:
	~Configer();

  public:
	static Configer *GetInstance();

  public:
	bool Load(const char *pConfName); //加载配置文件

	void SetConfigInfo(const std::vector<PConfItem>& config);

	std::vector<PConfItem> GetConfigInfo() const;

  private:
	static Configer *mInstance;
	std::vector<PConfItem> mConfigInfo;


	class GarCollector
	{
	  public:
		~GarCollector();
	};
};

#endif 
