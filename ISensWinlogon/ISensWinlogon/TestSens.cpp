#define _WIN32_IE 0x0500

#include "..\..\Common\CommonHead.h"
#include "..\..\Common\MyPrintTool.cpp"
#include <eventsys.h>
#include <Sensevts.h>
#include <stdio.h>
#include <iostream>


static wchar_t* methods[] = { L"Logon", L"Logoff", L"StartShell", L"DisplayLock", L"DisplayUnlock", L"StartScreenSaver", L"StopScreenSaver" };
static wchar_t* names[] = { L"HB_System_Logon", L"HB_System_Logoff", L"HB_StartShell", L"HB_DisplayLock", L"HB_DisplayUnlock",
L"HB_StartScreenSaver", L"HB_StopScreenSaver" };


static wchar_t* subids[] = { 
	L"{561D0791-47C0-4BC3-87C0-CDC2621EA653}",
	L"{12B618B1-F2E0-4390-BADA-7EB1DC31A70A}",
	L"{5941931D-015A-4F91-98DA-81AAE262D090}",
	L"{B7E2C510-501A-4961-938F-A458970930D7}", 
	L"{11305987-8FFC-41AD-A264-991BD5B7488A}",
	L"{6E2D26DF-0095-4EC4-AE00-2395F09AF7F2}", 
	L"{F53426BC-412F-41E8-9A5F-E5FA8A164BD6}"
	};

//interface MyISensLogon : public IDispatch
interface MyISensLogon : public ISensLogon//IDispatch
{
private:
	long ref;

public:
	MyISensLogon()
	{
		ref = 1;
	}

	STDMETHODIMP QueryInterface(REFIID iid, void ** ppv)
	{
		if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IDispatch) || IsEqualIID(iid, IID_ISensLogon)) {
			*ppv = this;
			AddRef();
			return S_OK;
		}

		*ppv = NULL;
		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef()
	{
		return InterlockedIncrement(&ref);
	}

	ULONG STDMETHODCALLTYPE Release()
	{
		int tmp = InterlockedDecrement(&ref);
		return tmp;
	}

	HRESULT STDMETHODCALLTYPE GetTypeInfoCount(unsigned int FAR* pctinfo)
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE GetTypeInfo(unsigned int iTInfo, LCID lcid, ITypeInfo FAR* FAR* ppTInfo)
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, OLECHAR FAR* FAR* rgszNames, unsigned int cNames, LCID lcid, DISPID FAR* rgDispId)
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS FAR* pDispParams, VARIANT FAR* parResult, EXCEPINFO FAR* pExcepInfo, unsigned int FAR* puArgErr)
	{
		return E_NOTIMPL;
	}

	// ISensLogon methods:
	virtual HRESULT STDMETHODCALLTYPE Logon(BSTR bstrUserName)
	{
		MyDebugPrint(TEXT("Logon"));
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Logoff(BSTR bstrUserName)
	{
		MyDebugPrint(TEXT("Logoff"));
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE StartShell(BSTR bstrUserName)
	{
		MyDebugPrint(TEXT("StartShell"));
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE DisplayLock(BSTR bstrUserName)
	{
		OutputDebugStringA("[ZGHTEST] 1  DisplayLock");
		MyDebugPrint(TEXT("DisplayLock"));
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE DisplayUnlock(BSTR bstrUserName)
	{
		OutputDebugStringA("[ZGHTEST] 2  DisplayUnlock");
		MyDebugPrint(TEXT("DisplayUnlock"));
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE StartScreenSaver(BSTR bstrUserName)
	{
		MyDebugPrint(TEXT("StartScreenSaver"));
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE StopScreenSaver(BSTR bstrUserName)
	{
		MyDebugPrint(TEXT("StopScreenSaver"));
		return S_OK;
	}
};

//********************************************************************************

//************************** Function WndProc  ***********************************
HRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	static MyISensLogon *pISensLogon;
	static IEventSystem *pIEventSystem = 0;
	static int counter = 0;
	static UINT_PTR timer_id;
	HRESULT res;
	static HMODULE hDLL;
	int i;
	int errorIndex;
	wchar_t nom[64];

	switch (message)
	{
	case WM_NCCREATE:
	case WM_NCACTIVATE:
		return 1;
	case WM_CREATE:
	{
		// Declarations:
		IEventSubscription*         pIEventSubscription = 0;

		// Obtain an interface of the object CEventSystem:
		res = CoCreateInstance(CLSID_CEventSystem, 0, CLSCTX_SERVER, IID_IEventSystem, (void**)&pIEventSystem);

		// Create an instance of our interface MyISensNetwork:
		pISensLogon = new MyISensLogon();

		// Sign up to receive the notifications required:
		for (i = 0; i<7; i++)
		{
			// Get the interface object IEventSubscription CEventSubscription:
			res = CoCreateInstance(CLSID_CEventSubscription, 0, CLSCTX_SERVER, IID_IEventSubscription, (LPVOID*)&pIEventSubscription);

			// Specify the event class to receive:
			res = pIEventSubscription->put_EventClassID(OLESTR("{d5978630-5b9f-11d1-8dd2-00aa004abd5e}"));

			// Specify the interface to receive notifications:
			res = pIEventSubscription->put_SubscriberInterface((IUnknown*)pISensLogon);

			// Indicate the method that will be called:
			res = pIEventSubscription->put_MethodName(methods[i]);

			// Give a name to the entry:
			res = pIEventSubscription->put_SubscriptionName(names[i]);

			// Provide a unique identifier for the event:
			res = pIEventSubscription->put_SubscriptionID(subids[i]);

			// from ISensLogon Code
			res = pIEventSubscription->put_PerUser(TRUE);

			// Register registration:
			res = pIEventSystem->Store(PROGID_EventSubscription, (IUnknown*)pIEventSubscription);

			// Release the interface IEventSubscription:
			res = pIEventSubscription->Release();
			pIEventSubscription = 0;
		}

		//timer_id = SetTimer(hwnd, 10, 10000, NULL);
		return 0;
	}

	case WM_TIMER:
		if (counter++ == 2)
		{
			KillTimer(hwnd, timer_id);
			lstrcpyW(nom, L"SubscriptionID=");

			for (i = 0; i < 7; ++i)
			{
				lstrcatW(nom + 15, subids[i]);
				res = pIEventSystem->Remove(PROGID_EventSubscription, nom, &errorIndex);
				nom[15] = 0;
			}

			pIEventSystem->Release();
			OutputDebugStringA("delete pISensLogon;");   // 这里先删除
			delete pISensLogon;
			PostQuitMessage(0);
			return 1;
		}
		return 1;

	default:
		break;
	}
	return 0;
}

//************************ Fonction WinMain ***************************************
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
	BOOL ret;
	HRESULT res;
	//
	//// Initialiser la librairie COM:
	res = CoInitialize(0);
	if (!SUCCEEDED(res))
	{
		OutputDebugStringA("CoInitialize failed");
	}
	WNDCLASS wc = { 0,WndProc,0,0,hInst,0,0,0,0,TEXT("invisible") };
	RegisterClass(&wc);
	
	CreateWindow(TEXT("invisible"), TEXT(""), 0, 0, 0, 0, 0, 0, 0, 0, 0);
	// Boucle des messages:
	MSG Msg;
	while (GetMessage(&Msg, 0, 0, 0))
	{
		DispatchMessage(&Msg);
	}
											
	CoUninitialize(); // 这个是最后反注册com组件
	return 0;
}