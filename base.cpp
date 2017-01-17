#include "base.h"

volatile bool g_ServerQuit = false;
volatile bool g_ClientQuit = false;

BYTE g_NETDATA[g_NETDATA_SIZE] = { 0 };

CRITICAL_SECTION g_csConsole;

void WriteToConsole(char *szFormat, ...)
{
	EnterCriticalSection(&g_csConsole);

	va_list args;
	va_start(args, szFormat);

	vprintf(szFormat, args);

	va_end(args);

	LeaveCriticalSection(&g_csConsole);
}

DWORD IOCPBase::sm_NumRecvThreads = 1;


unsigned __stdcall RecvMain(void* data)
{
	IOCPBase* iocpBase = (IOCPBase*)data;
	iocpBase->RecvThread();
	return 0;
}


void IOCPBase::CreateIOCP()
{
	m_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	if (NULL == m_hIOCompletionPort)
	{
		m_hIOCompletionPort = INVALID_HANDLE_VALUE;
		throw SYS_EXCEPTION;
	}
}

void IOCPBase::CreateRecvThreadPool()
{
	m_RecvThreads.resize(sm_NumRecvThreads);
	//Create worker threads
	for (DWORD c = 0; c < sm_NumRecvThreads; c++)
	{
		uintptr_t thread = _beginthreadex(NULL, 1024 * 1024 * 2, &RecvMain, this, 0, NULL);
		
		if (thread == 0)
		{
			//остановить потоки
			m_RecvThreads.clear();
			throw SYS_EXCEPTION;
		}

		m_RecvThreads[c] = thread;
	}
}

void IOCPBase::DestroyRecvThreadPool()
{
	//
}

void IOCPBase::DestroyIOCP()
{

}

IOCPBase::IOCPBase() : m_hIOCompletionPort(INVALID_HANDLE_VALUE)
{

}

IOCPBase::~IOCPBase()
{
	DestroyRecvThreadPool();
	DestroyIOCP();
}
