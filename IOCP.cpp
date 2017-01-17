// IOCP.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"

#include "server.h"
#include "client.h"


uintptr_t g_Threads[] = { NULL, NULL };
DWORD g_ThreadsNum = sizeof(g_Threads) / sizeof(decltype(g_Threads[0]));


#define RETURN  { char symbol; cin >> symbol; } return










int main()
{
	WSADATA wsaData;
	ZeroMemory(&wsaData, sizeof(wsaData));

	int nResult;
	nResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (NO_ERROR != nResult)
	{
		cout << "nError occurred while executing WSAStartup()" << endl;
		RETURN -1; //error
	}
	
	ZeroMemory(&g_csConsole, sizeof(CRITICAL_SECTION));

	InitializeCriticalSection(&g_csConsole);

	g_Threads[0] = _beginthreadex(NULL, 1024 * 1024 * 2, ServerMain, NULL, 0, NULL);
	if (g_Threads[0] == 0)
	{
		cout << "nError occurred while executing _beginthreadex()" << endl;
		RETURN -1;
	}

	g_Threads[1] = _beginthreadex(NULL, 1024 * 1024 * 2, ClientMain, NULL, 0, NULL);
	if (g_Threads[1] == 0)
	{
		cout << "nError occurred while executing _beginthreadex()" << endl;
		RETURN - 1;
	}
	
	{
		char symbol;
		cin >> symbol;
		g_ServerQuit = true;
		g_ClientQuit = true;
	}

	DWORD result = WaitForMultipleObjects(g_ThreadsNum, (HANDLE*)g_Threads,TRUE, INFINITE);

	if (result == WAIT_FAILED)
	{
		cout << "WaitForMultipleObjects error " << result << " GetLastError " << GetLastError();
		RETURN -1; //error
	}
	

	DeleteCriticalSection(&g_csConsole);
	
	cout << "Success finished!" << endl;

	RETURN 0;
}

