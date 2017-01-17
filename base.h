#pragma once

#include "exceptions.h"

const int SERVER_PORT = 5500;

unsigned __stdcall RecvMain(void* data);
extern CRITICAL_SECTION g_csConsole;


void WriteToConsole(char *szFormat, ...);


extern volatile bool g_ServerQuit;
extern volatile bool g_ClientQuit;


const DWORD g_NETDATA_SIZE = 50*1024;
extern BYTE g_NETDATA[g_NETDATA_SIZE];




class IOCPBase
{	
protected:
	HANDLE m_hIOCompletionPort;

	static DWORD sm_NumRecvThreads;
	vector<uintptr_t> m_RecvThreads;	
		
	friend unsigned __stdcall RecvMain(void* data);
	virtual void RecvThread() = 0;
		
	void CreateIOCP();
	void CreateRecvThreadPool();	
	
	void DestroyRecvThreadPool();
	void DestroyIOCP();

	
	IOCPBase();
	virtual ~IOCPBase();
};