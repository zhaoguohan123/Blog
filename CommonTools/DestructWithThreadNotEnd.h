#ifndef _DESTRUCT_WITH_THREAD_NOT_END_
#define _DESTRUCT_WITH_THREAD_NOT_END_

#include "CommonHead.h"

// 1. 创建一个线程每隔1s打印类内的变量，当主线程10s后调用析构函数后，类的对象释放了，线程再去访问对象的变量就崩溃了
namespace DestructWithThreadNotEnd
{
	class TestClass;

	void fun2(TestClass *);

	class TestClass
	{
	public:
		TestClass();
		~TestClass();
		std::string _test;
	private:
	};

	TestClass::TestClass():
		_test("test_class")
	{
		MyDebugPrintA("TestClass construct run!! \n");

		// 析构的时候创建一个线程
		boost::shared_ptr<boost::thread>  thread_1 = boost::make_shared<boost::thread>(boost::bind(fun2, this));
		
		thread_1->detach();// deatch不能解决崩溃问题，线程阻塞，没有使用成员变量时，退出进程，线程也就不存在了
	}

	TestClass::~TestClass()
	{
		MyDebugPrintA("~TestClass construct run!! \n");
	}

	void fun2(TestClass* pObj)
	{
		do
		{
			Sleep(3000);
			MyDebugPrintA(pObj->_test.data());
		} while (true);
	}

	void main() 
	{
		TestClass* pObj = new TestClass();
		if (pObj == NULL)
		{
			MyDebugPrintA(" new TestClass failed!");
			return;
		}
		Sleep(10000);
		MyDebugPrintA(" delete pObj!");
		if (pObj)
		{
			delete pObj;
			pObj = NULL;
		}
	}
}



#endif // !_DESTRUCT_WITH_THREAD_NOT_END_
