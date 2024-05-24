#pragma once
#include <windows.h>

class CSingletonApplication
{
	////类型声明
public:
	typedef CSingletonApplication my_type;
	////构造、析构函数
public:
	//构造函数
	CSingletonApplication(void) : m_hSemaphore(0) {}
	//析构函数
	~CSingletonApplication(void)
	{
		CloseHandle(m_hSemaphore);
		m_hSemaphore = 0;
	}
private:
	//拷贝构造函数(声明未实现,禁止value语义)
	CSingletonApplication(const my_type&);
	//赋值操作符重载(声明未实现,禁止value语义)
	my_type& operator = (const my_type&);
	////接口函数
public:
	//是否有前实例
	static bool HasPreviousInstance(void);
	//注册当前实例
	void RegisterCurrentInstance(void);
	////执行函数
private: 
	//得到单实例信号名称
	static const char* GetApplicationSemaphoreName(void) { return "929011BD-2F62-47C4-87F3-F863D400152F"; }
	////数据成员
private:
	HANDLE	m_hSemaphore;

};

bool CSingletonApplication::HasPreviousInstance(void)
{
	HANDLE hSemaphore = ::CreateSemaphoreA(NULL, 1, 1, GetApplicationSemaphoreName());
	bool bExist = (GetLastError() == ERROR_ALREADY_EXISTS);
	if (hSemaphore)
		CloseHandle(hSemaphore);
	return bExist;
}

void CSingletonApplication::RegisterCurrentInstance(void)
{
	//assert(!HasPreviousInstance());
	if (HasPreviousInstance())
	{
		return;
	}
	m_hSemaphore = ::CreateSemaphoreA(NULL, 1, 1, GetApplicationSemaphoreName());
	//assert(HasPreviousInstance());
	if (!HasPreviousInstance())
	{
		return;
	}
}