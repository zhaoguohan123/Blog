#pragma once
#include "framework.h"

class CHelpCoInitialize
{
public:
	CHelpCoInitialize() : m_hr(OleInitialize(nullptr)) {}
	~CHelpCoInitialize() 
	{
		if (SUCCEEDED(m_hr))
		{
			OleUninitialize();
		}
	}
	operator HRESULT() const { return m_hr; }
	HRESULT m_hr;
};