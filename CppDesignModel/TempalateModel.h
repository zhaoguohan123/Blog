#ifndef _TEMPALATE_MODEL_HEADER_
#define _TEMPALATE_MODEL_HEADER_
#include "utils.h"

using namespace std;
// ģ�巽��ģʽ: ����һ�������е��㷨�ĹǼ�(�ȶ�)������һЩ����(�仯)�ӳٵ������С�
// ʹ��������Բ��ı�һ���㷨�Ľṹ�����ض�����㷨��ĳЩ�ض����衣

// δʹ��ģ�巽��ģʽ
class UnTemplateClass
{
public:	
	void step1() { cout << "step1" << endl; };
	void step3() { cout << "step3" << endl; };
	void step5() { cout << "step5" << endl; };
};

// ����һ����󶨵ķ�����Ӧ�ÿ�����Ա��Ҫ�������������
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

// ʹ��ģ�巽��ģʽ
class TemplateClass
{
public:
	void run() {
		step1();
		step2();   // ֧�ֱ仯 �� �麯���Ķ�̬����
		step3();
		step4();   // ֧�ֱ仯 �� �麯���Ķ�̬����
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


// ������ģ�巽���� step1,3,5���ȶ��ģ�����ģ���step2,4�Ǳ仯�ģ����Խ�step2,4����Ϊ�麯����������ʵ�֣������Ϳ���ʵ�ֱ仯�Ĳ��ֲ�Ӱ���ȶ��Ĳ��֡�

#endif // !_TEMPALATE_MODEL_HEADER_

