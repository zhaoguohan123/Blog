#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <chrono>
#include <ctime>
#include <set>
#include "ScreenShot.h"
#include "util.h"
#include "AppComm.h"

using namespace std;

// ͼƬ�����ַ�
const string securityWords = "��������";

// ���̺ڰ�������Ŀǰֻע��
vector<wstring> processNameVec = {
	L"msedge.exe",L"inject.exe"
};

// ���̶�����
vector<wstring> processVec = {
	L"mailmaster.exe",L"msedge.exe"/*,L"spoolsv.exe",L"explorer",L"winlogon.exe" ,
	L"vagent.exe",L"vdagent.exe",L"vservice.exe",L"vdservice.exe",
	L"conhost.exe",L"csrss.exe",L"dwm.exe",L"wininit.exe",L"lsass.exe",
	L"wmiapsrv.exe",L"userinit.exe",L"rundll32.exe",L"smartscreen.exe",L"runtimebroker.exe",L"shellexperiencehost.exe",L"searchui.exe",
	L"taskmgr.exe",L"nissrv.exe",L"searchIndexer.exe",L"msdtc.exe",L"msmpeng.exe",L"vgauthservice.exe",L"flserver.exe",L"sgrmbroker.exe",
	L"searchfilterHost.exe",L"fontdrvhost.exe",L"compmgmtlauncher.exe",L"backgroundtaskHost.exe",L"taskhostw.exe",L"mmc.exe",
	L"wmiadap.exe",L"securityhealthhost.exe"*/
};

// IP������
vector<string> ipVec = { /*"127.0.0.1",*/"192.168.133.1", "172.24.109.12", "172.19.19.0" };

// ��ַ������
vector<string> webVec = {
		"www.baidu.com","www.qq.com","dh.aabyss.cn","www.subaibaiys.com","www.huya.com","dashi.163.com",
		"mail.aliyun.com","mail.sina.com.cn","www.csdn.net","gaze.run","www.zhihu.com","cn.voicedic.com",
		 "www.logosc.cn","fonts.safe.360.cn","www.alltoall.net","www.iqiyi.com",
		"yidanshu.com","www.ixinqing.com","anthems.lidicity.com","zh.wikihow.com","www.skypixel.com"
};

// ������ַ��ر�
unordered_map<string, wstring> hostTextMap = {
	{"dashi.163.com",L"�����ʦ"},{"mail.163.com",L"�����ʦ"},{"mail.qq.com",L"QQ����"}
};

inline std::string wstring2string(std::wstring wstr)
{
	std::string result;
	int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
	if (len <= 0)return result;
	char* buffer = new char[len + 1];
	if (buffer == NULL)return result;
	WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), buffer, len, NULL, NULL);
	buffer[len] = '\0';
	result.append(buffer);
	delete[] buffer;
	return result;
}

// ��ͼ�߳�
void ScreenShot()
{
	AppComm* appCom = AppComm::GetInstance();
	if (!appCom->isSuccess()) return;

	// ��ǰĿ¼�´���һ��shotĿ¼�����ڴ�Ž�ͼ
	if (!CreateDirectoryW(L".\\shot", NULL))
	{
		DWORD error = GetLastError();
		if (error == ERROR_ALREADY_EXISTS) std::cout << "Ŀ¼�Ѵ���" << endl;
		else std::cerr << "Ŀ¼����ʧ�ܣ������룺" << error << std::endl;
	}

	wstring windowText;
	while (true)
	{
		//printf("wait...\n");
		// �¼��ȴ�
		WaitForSingleObject(appCom->GetHitWebEvent(), INFINITE);

		wstring imagePath;
		string ocrPath;
		vector<string> orcWordsVec;
		MainCapScrren(windowText, imagePath);
		ocrPath = wstring2string(imagePath);

		//orcWordsVec = GetImageWords(ocrPath);

		SetEvent(appCom->GetHitWebEvent());

		ResetEvent(appCom->GetHitWebEvent());

		// �ڶ�����֤���жϻ���ڱ����ٽ�ͼ�����κ�̨�����ʼ���ַ��ȡ��������ͼƬ
		//HWND activeWindow = 0;
		//windowText = Util::GetForeWindowsText(activeWindow);
		//wstring processName = Util::GetProcessNameByHwnd(activeWindow);
		//if (!windowText.empty())
		//{
		//	// ������ж�
		//	for (auto hostAndText : hostTextMap)
		//	{
		//		if (windowText.find(hostAndText.second) != wstring::npos)
		//		{
		//			wprintf(L"��ǰ�����: %ls  ---  ��Ӧ����Ϊ: %ls\n", windowText.c_str(), processName.c_str());
		//			MainCapScrren(windowText);
		//			appCom->BlockMailWeb(TRUE);
		//		}
		//		else
		//		{
		//			wprintf(L" --- ���������� --- \n");
		//			appCom->BlockMailWeb(FALSE);
		//		}

		//		SetEvent(appCom->GetHitWebEvent());
		//		Sleep(100);
		//		ResetEvent(appCom->GetHitWebEvent());
		//	}
		//}
	}
}

// ���˹��������߳�
void CommThreadFunc()
{
	ULONG loop = 0;
    AppComm* appCom = AppComm::GetInstance();
    if (!appCom->isSuccess()) return;

	// ����������֪ͨ�¼�
	//appCom->SetMailWebEvent();
	//appCom->SetRecordTimeEvent();
#if 0
	// ����Ҫ��ͼ����ַ
	for (auto hostAndText : hostTextMap)
	{
		vector<ULONG> ipVec = Util::GetIpByHost(hostAndText.first);
		for (auto ip : ipVec)
		{
			FILTER filter{ 0 };
			filter.filterIndex = SCREEN_SHOT;
			filter.ip = ip;
			appCom->SetFilter(&filter, sizeof(FILTER));
			printf("%d.%d.%d.%d\n", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip) & 0xFF);
		}
	}
#endif
	// ��������IP
	for (auto charIp : ipVec)
	{
		FILTER filter{ 0 };
		filter.filterIndex = IP_BLOCK;
		filter.ip = Util::CharToIP(charIp);
		appCom->SetFilter(&filter, sizeof(FILTER));
		printf("block IP is: %s\n", charIp.c_str());
	}
#if 0
	// ���������ؽ���,����������������̫��,�����������ַǰ���ȡ����ip
	for (auto process : processVec)
	{
		FILTER filter{ 0 };
		filter.filterIndex = PROCESS_BLOCK;
		memcpy(filter.processName, process.c_str(), process.size() * sizeof(WCHAR));
		appCom->SetFilter(&filter, sizeof(FILTER));
		wprintf(L"process: %s \n", process.c_str());
	}
#endif
	Sleep(200);
}


void DropAllIpFilter()
{
    AppComm* appCom = AppComm::GetInstance();
    if (!appCom->isSuccess()) return;

	appCom->DropAllFilter();
}

// ��������ʱ���߳�
shared_mutex mtx;
unordered_map<HANDLE, pair<wstring, PROCESS_TIME_RECORD>> recordMap;
void ProcessTime()
{
	Sleep(3000);

	AppComm* appCom = AppComm::GetInstance();
	if (!appCom->isSuccess()) return;

	// ����PID��ȡ(���������б�) [��ʼ -- ��ǰ] ʱ��
	PROCESS_TIME_RECORD prcessRecord = { 0 };
	unordered_map<HANDLE, wstring> namePidMap = Util::GetAllProcessNameAndId();
	for (const auto& namePid : namePidMap)
	{
		if (appCom->GetProcessStartCurTime(namePid.first, &prcessRecord, sizeof(PROCESS_TIME_RECORD)))
		{
			unique_lock<shared_mutex> lock(mtx);
			recordMap[prcessRecord.processID] = std::make_pair(namePid.second, prcessRecord);
		}
	}

	// ����֪ͨ  [��ʼ -- ����|��ǰ] ʱ��
	PROCESS_TIME_RECORD processTime = { 0 };
	while (true)
	{
		WaitForSingleObject(appCom->GetRecordTimeEvent(), INFINITE);
		appCom->GetRecordTimeInfo(&processTime, sizeof(PROCESS_TIME_RECORD));
		SetEvent(appCom->GetRecordTimeEvent());

		if (processTime.processID)
		{
			// ��ӻ���ĵ�����Map��
			unique_lock<shared_mutex> lock(mtx);
			if (recordMap.find(processTime.processID) != recordMap.end())
			{
				recordMap[processTime.processID] = make_pair(recordMap[processTime.processID].first, processTime);
			}
			else
			{
				wstring processName = Util::GetProcessNameByProcessId(processTime.processID);
				if (processName.empty()) continue;
				pair<wstring, PROCESS_TIME_RECORD> newPair = make_pair(processName, processTime);
				recordMap[processTime.processID] = newPair;
			}
		}
		ResetEvent(appCom->GetRecordTimeEvent());
	}
}

// ��ӡʱ�����
void PrintProcessTime()
{
	char start[80];
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::time_t now_c = std::chrono::system_clock::to_time_t(now);
	std::strftime(start, sizeof(start), "%Y-%m-%d %H:%M:%S", std::localtime(&now_c));

	AppComm* appCom = AppComm::GetInstance();
	if (!appCom->isSuccess()) return;

	// ����֪ͨ  [��ʼ -- ����|��ǰ] ʱ��
	ULONG count = 0;
	PROCESS_TIME_RECORD notifyTime = { 0 };
	PROCESS_TIME_RECORD prcessRecord = { 0 };
	SYSTEMTIME sysTime1 = { 0 };
	SYSTEMTIME sysTime2 = { 0 };
	while (true)
	{
		count++;
		WaitForSingleObject(appCom->GetRecordTimeEvent(), INFINITE);

		appCom->GetRecordTimeInfo(&notifyTime, sizeof(PROCESS_TIME_RECORD));
		SetEvent(appCom->GetRecordTimeEvent());
		if (notifyTime.endFlag)
		{
			FILETIME* lpFileTime1 = (FILETIME*)&notifyTime.startTime;
			FILETIME* lpFileTime2 = (FILETIME*)&notifyTime.endTime;
			FileTimeToSystemTime(lpFileTime1, &sysTime1);
			FileTimeToSystemTime(lpFileTime2, &sysTime2);

			if (notifyTime.isValid)
			{
				printf("[info]:pid is:%lld \t\t StartTime:%4u-%02u-%02u %02u:%02u:%02u  EndTime:%4u-%02u-%02u %02u:%02u:%02u\n",
					(ULONG_PTR)notifyTime.processID,
					sysTime1.wYear, sysTime1.wMonth, sysTime1.wDay,
					sysTime1.wHour, sysTime1.wMinute, sysTime1.wSecond,
					sysTime2.wYear, sysTime2.wMonth, sysTime2.wDay,
					sysTime2.wHour, sysTime2.wMinute, sysTime2.wSecond
				);
			}
			else
			{
				printf("[info]: pid is:%lld ��ȡʱ����Ϣ����\n", (ULONG_PTR)notifyTime.processID);
			}
		}
		else
		{
			appCom->GetProcessStartCurTime(notifyTime.processID, &prcessRecord, sizeof(PROCESS_TIME_RECORD));

			FILETIME* lpFileTime1 = (FILETIME*)&prcessRecord.startTime;
			FILETIME* lpFileTime2 = (FILETIME*)&prcessRecord.curTime;
			FileTimeToSystemTime(lpFileTime1, &sysTime1);
			FileTimeToSystemTime(lpFileTime2, &sysTime2);

			if (prcessRecord.endFlag == FALSE)
			{
				printf("[info]:pid is:%lld \t\t StartTime:%4u-%02u-%02u %02u:%02u:%02u  CurTime:%4u-%02u-%02u %02u:%02u:%02u\n",
					(ULONG_PTR)prcessRecord.processID,
					sysTime1.wYear, sysTime1.wMonth, sysTime1.wDay,
					sysTime1.wHour, sysTime1.wMinute, sysTime1.wSecond,
					sysTime2.wYear, sysTime2.wMonth, sysTime2.wDay,
					sysTime2.wHour, sysTime2.wMinute, sysTime2.wSecond
				);
			}
			else
			{
				if (prcessRecord.isValid == FALSE)
				{
					printf("[info]:����������...\n");
				}
				printf("[info]:pid is:%lld ��������,�޷���ȡ�������̵ĵ�ǰʱ��\n", (ULONG_PTR)prcessRecord.processID);
				system("taskkill /f /t /pid 40744");
				system("pause");
			}
		}
		if (count == 15)
		{
			system("cls");

			count = 0;
			char current[80];
			std::time_t c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			std::strftime(current, sizeof(current), "%Y-%m-%d %H:%M:%S", std::localtime(&c));

			cout << " ======= time record start system Time is:[" << start << "]  current system Time is:[" << current << "] ======= " << endl;
		}
		ResetEvent(appCom->GetRecordTimeEvent());
	}
}

// ���̺ڰ������߳�
void ProcessProHibition()
{
	AppComm* appCom = AppComm::GetInstance();
	if (!appCom->isSuccess()) return;

	while (true)
	{
		WaitForSingleObject(appCom->GetProhibitEvent(), INFINITE);

		PROCESS_CREATE_INFO createInfo = { 0 };
		appCom->GetCreateProcessInfo(&createInfo, sizeof(PROCESS_CREATE_INFO));

		for (auto processName : processNameVec)
		{
			if (createInfo.fileName && wcscmp(createInfo.fileName, processName.c_str()) == 0)
			{
				PROCESS_RUNTIME_STAT stat = { 0 };
				stat.isProhibitCreate = FALSE;
				stat.isInject = TRUE;
				appCom->SetProcessRuntimeStat(&stat, sizeof(PROCESS_RUNTIME_STAT));
			}
		}

		SetEvent(appCom->GetProhibitEvent());
		ResetEvent(appCom->GetProhibitEvent());
	}
}

// ������ַIP
void ReflushWebSite()
{
	Sleep(500);

	AppComm* appComm = AppComm::GetInstance();
	if (!appComm->isSuccess()) return;

	// ��ַ���أ��Ƚ���ַתΪIP��������IP����
	unordered_map<string, set<ULONG>> webIpContiner;
	for (auto web : webVec)
	{
		vector<ULONG> ipVec = Util::GetIpByHost(web);
		if (!ipVec.empty())
		{
			for (auto ip : ipVec)
			{
				webIpContiner[web].insert(ip);
				FILTER filter{ 0 };
				filter.filterIndex = WEB_BLOCK;
				filter.ip = ip;
				appComm->SetFilter(&filter, sizeof(FILTER));
				printf("website %s --- add newIp is %u.%u.%u.%u \n", web.c_str(),(ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip) & 0xFF);
			}
		}
		else
		{
			printf("First GetIpByHost fail\n");
		}
	}

	// ����ʱ��ˢ��������ַ
	auto websiteRefreshThread = [](unordered_map<string, set<ULONG>> webIpContiner, vector<string> webVec)
	{
		AppComm* appComm = AppComm::GetInstance();
		std::set<ULONG> tmpIpSet;
		bool refFlag;
		while (true)
		{
			for (auto web : webVec)
			{
				vector<ULONG> ipVec = Util::GetIpByHost(web);
				if (!ipVec.empty())
				{
					for (auto newIp : ipVec)
					{
						if (webIpContiner[web].find(newIp) == webIpContiner[web].end())
						{
							tmpIpSet.insert(newIp);
						}
					}
					if (!tmpIpSet.empty())
					{
						printf("website refresh %s \n", web.c_str());
						for (auto oldIp : webIpContiner[web])
						{
							FILTER filter{ 0 };
							filter.filterIndex = WEB_BLOCK;
							filter.ip = oldIp;
							appComm->DropFilter(&filter, sizeof(FILTER));
							printf("drop Ip is %u.%u.%u.%u \n", (oldIp >> 24) & 0xFF, (oldIp >> 16) & 0xFF, (oldIp >> 8) & 0xFF, (oldIp) & 0xFF);
						}
						webIpContiner[web].clear();
						webIpContiner[web] = tmpIpSet;
						tmpIpSet.clear();
						for (auto newIp : webIpContiner[web])
						{
							FILTER filter{ 0 };
							filter.filterIndex = WEB_BLOCK;
							filter.ip = newIp;
							appComm->SetFilter(&filter, sizeof(FILTER));
							printf("add newIp is %u.%u.%u.%u \n", (newIp >> 24) & 0xFF, (newIp >> 16) & 0xFF, (newIp >> 8) & 0xFF, (newIp) & 0xFF);
						}
					}
				}
			}
			std::this_thread::sleep_for(std::chrono::seconds(3));
		}
	};
	thread(websiteRefreshThread, webIpContiner, webVec).detach();
}

void AddIPWhiteList()
{
    CommThreadFunc();

    //thread t11(ReflushWebSite);
    //t11.detach();

    //thread t2(ScreenShot);
    //t2.detach();

    //thread t3(ProcessTime);
    //t3.detach();

    //thread t4(PrintProcessTime);
    //t4.detach();

    //thread t5(ProcessProHibition);
    //t5.detach();

//     std::cout << "start DropAllIpFilter" << std::endl;
//     DropAllIpFilter();
//     system("pause");
}

void BlockAccessDns(BOOLEAN enalbe)
{
	AppComm* appCom = AppComm::GetInstance();
	appCom->BlockAccessDns(enalbe);
}

void EnableIPWhiteList(BOOLEAN enable)
{
    AppComm* appCom = AppComm::GetInstance();
    appCom->EnableIPWhiteList(enable);
}

int main()
{
	setlocale(LC_ALL, "en_US.UTF-8");
	std::cout << "start CommThreadFunc" << std::endl;
    AppComm* appCom = AppComm::GetInstance();
    appCom->Initialize();

    std::cout << "-------------Enable BlockAccessDns and Disable IPWhiteList----------------" << std::endl;
    BlockAccessDns(1);
	EnableIPWhiteList(0);
    system("pause");

    std::cout << "-------------Enable BlockAccessDns and Add IP White List----------------" << std::endl;
	BlockAccessDns(1);
	EnableIPWhiteList(1);
    AddIPWhiteList();
	system("pause");

	std::cout << "-------------Disable BlockAccessDns and Disable IPWhiteList----------------" << std::endl;
    BlockAccessDns(0);
	EnableIPWhiteList(0);

	appCom->UnInitlize();

	return 0;
}