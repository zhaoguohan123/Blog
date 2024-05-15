#pragma once
#include <cassert>
#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <GdiPlus.h>
#include <thread>
#include <mutex>
#include <string>

void MainCapScrren(const std::wstring& titleName, OUT std::wstring& imagePath);