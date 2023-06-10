#ifndef _TEMPALATE_MODEL_HEADER_
#define _TEMPALATE_MODEL_HEADER_
#include "utils.h"

using namespace std;
// 模板方法模式: 定义一个操作中的算法的骨架(稳定)，而将一些步骤(变化)延迟到子类中。
// 使得子类可以不改变一个算法的结构即可重定义该算法的某些特定步骤。

// 未使用模板方法模式
class UnTemplateClass
{
public:	
	void step1() { cout << "step1" << endl; };
	void step3() { cout << "step3" << endl; };
	void step5() { cout << "step5" << endl; };
};

// 这是一种早绑定的方法，应用开发人员需要参与程序主流程
void  CallFunc() 
{
	UnTemplateClass* p = new UnTemplateClass();
	p->step1();
	cout << " step2" << endl;
	p->step3();
	cout << " step4" << endl;
	p->step5();
}



/*----------------------------------------------------------------------------------------------------*/

// 使用模板方法模式
class TemplateClass
{
public:
	void run() {
		step1();
		step2();   // 支持变化 ： 虚函数的多态调用
		step3();
		step4();   // 支持变化 ： 虚函数的多态调用
		step5();
	}
	void step1() { cout << "step1" << endl; };
	void step3() { cout << "step3" << endl; };
	void step5() { cout << "step5" << endl; };

	virtual void step2() = 0;
	virtual void step4() = 0;

	virtual ~TemplateClass() {};
};

class UserFunc:public TemplateClass
{
public:
	virtual void step2() { cout << " my test2" << endl; };
	virtual void step4() { cout << " my test2" << endl; };
};

void  CallFunc2()
{
	TemplateClass *  p = new UserFunc();
	p->step1();
	p->step2();
	p->step3();
	p->step3();
	p->step5();
}


// 如何理解模板方法： step1,3,5是稳定的，不变的，而step2,4是变化的，所以将step2,4定义为虚函数，由子类实现，这样就可以实现变化的部分不影响稳定的部分。

#endif // !_TEMPALATE_MODEL_HEADER_

