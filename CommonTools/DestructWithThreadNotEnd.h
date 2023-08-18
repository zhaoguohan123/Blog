#ifndef _DESTRUCT_WITH_THREAD_NOT_END_
#define _DESTRUCT_WITH_THREAD_NOT_END_

#include "CommonHead.h"

// 1. ����һ���߳�ÿ��1s��ӡ���ڵı����������߳�10s�����������������Ķ����ͷ��ˣ��߳���ȥ���ʶ���ı����ͱ�����
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

		// ������ʱ�򴴽�һ���߳�
		boost::shared_ptr<boost::thread>  thread_1 = boost::make_shared<boost::thread>(boost::bind(fun2, this));
		
		thread_1->detach();// deatch���ܽ���������⣬�߳�������û��ʹ�ó�Ա����ʱ���˳����̣��߳�Ҳ�Ͳ�������
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
