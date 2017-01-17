// IOCP.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"

#include "Threads.h"
#include "Exceptions.h"
#include "base.h"
#include "server.h"
#include "client.h"


#define RETURN  { char symbol; cin >> symbol; } return

template <class T>
class ProgramThread : public Thread
{
public:
	T* m_ClientOrServer = NULL;
	virtual void ThreadMain() override
	{
		T clientOrServer;
		m_ClientOrServer = &clientOrServer;
		clientOrServer.ThreadMain();
		m_ClientOrServer = NULL;
	}
	virtual void SendQuit() override
	{
		ASSERT(m_ClientOrServer);
		m_ClientOrServer->m_Quit = true;
	}
};



int main()
{


	for (uint32_t c = 0, size = g_NETDATA_SIZE; c < size; c++)
	{
		g_NETDATA[c] = c;
	}

	cout << "s or c" << endl;

	char mode;
	cin >> mode;



	WSADATA wsaData;
	ZeroMemory(&wsaData, sizeof(wsaData));

	int nResult;
	nResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (NO_ERROR != nResult)
	{
		cout << "nError occurred while executing WSAStartup()" << endl;
		RETURN - 1; //error
	}

	ZeroMemory(&g_ConsoleCS, sizeof(CRITICAL_SECTION));

	InitializeCriticalSection(&g_ConsoleCS);

	Thread* thread;

	if (mode == 's')
	{
		thread = new ProgramThread<Server>;
	}
	else
	{		
		thread = new ProgramThread<Client>;
	}

	Thread::RunThread(*thread);

	{
		char symbol;
		cin >> symbol;		
		
	}

	thread->Quit();
	delete thread;

	


	DeleteCriticalSection(&g_ConsoleCS);
	
	cout << "Success finished!" << endl;

	RETURN 0;
}



int main2()
{
	try
	{

		for (uint32_t c = 0, size = g_NETDATA_SIZE; c < size; c++)
		{
			g_NETDATA[c] = c;
		}


		WSADATA wsaData;
		ZeroMemory(&wsaData, sizeof(wsaData));

		int nResult;
		nResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

		
		if (NO_ERROR != nResult)
		{
			throw SYS_EXCEPTION;			
		}

		ZeroMemory(&g_ConsoleCS, sizeof(CRITICAL_SECTION));

		InitializeCriticalSection(&g_ConsoleCS);

		auto serverThread = new ProgramThread<Server>;
		Thread::RunThread(*serverThread);
		Sleep(100);
		auto clientThread = new ProgramThread<Client>;
		Thread::RunThread(*clientThread);


		{
			char symbol;
			cin >> symbol;
		}

		clientThread->Quit();
		delete clientThread;
		serverThread->Quit();
		delete serverThread;


		DeleteCriticalSection(&g_ConsoleCS);

		cout << "Success finished!" << endl;

		return 0;

	}
	catch (SYS_EXCEPTION_TYPE& e)
	{		
		WriteToConsole("Main thread SYS_EXCEPTION %s", e.m_Message);
	}
	catch (std::exception& e)
	{
		WriteToConsole("Main thread std::exception %s", e.what());
	}

	return -1;

}