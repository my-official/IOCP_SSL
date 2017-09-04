
#include "stdafx.h"

#include "Threads.h"
#include "Exceptions.h"
#include "base.h"
#include "server.h"
#include "client.h"
#include "Transport.h"


#if OPENSSL_VERSION_NUMBER == 0x1000105fL

#if _MSC_VER >= 1900
FILE _iob[] = { *stdin, *stdout, *stderr };
extern "C" FILE * __cdecl __iob_func(void)
{
	return _iob;
}
#pragma comment (lib, "legacy_stdio_definitions.lib")
#endif

#endif



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
		assert(m_ClientOrServer);
		m_ClientOrServer->m_Quit = true;
	}
};


void ssl_init();



int main2()
{
	try
	{
		locale::global(locale(""));

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

		ssl_init();


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
	catch (std::exception& e)
	{
		WriteToConsole("Main thread std::exception %s", e.what());
	}
	catch (...)
	{
		WriteToConsole("Main thread unknown exception %s");
	}

	RETURN -1;
}



int main()
{
	try
	{
		locale::global(locale(""));

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
			throw EXCEPTION(SystemException());			
		}

		ZeroMemory(&g_ConsoleCS, sizeof(CRITICAL_SECTION));

		InitializeCriticalSection(&g_ConsoleCS);

		ssl_init();
		



		auto serverThread = new ProgramThread<Server>;
		Thread::RunThread(*serverThread);
		Sleep(500);
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
	catch (std::exception& e)
	{
		WriteToConsole("Main thread std::exception %s", e.what());
	}
	catch (...)
	{
		WriteToConsole("Main thread unknown exception %s");
	}

	return -1;

}


#if OPENSSL_VERSION_NUMBER == 0x1000105fL


typedef CRITICAL_SECTION	ssl_lock;

struct CRYPTO_dynlock_value
{
	ssl_lock lock;
};

int number_of_locks = 0;
ssl_lock *ssl_locks = nullptr;
SSL_CTX *ssl_ctx = nullptr;
ssl_lock lock_connect_ex;



void ssl_lock_callback(int mode, int n, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
		EnterCriticalSection(&ssl_locks[n]);
	else
		LeaveCriticalSection(&ssl_locks[n]);
}

CRYPTO_dynlock_value* ssl_lock_dyn_create_callback(const char *file, int line)
{
	CRYPTO_dynlock_value *l = (CRYPTO_dynlock_value*)malloc(sizeof(CRYPTO_dynlock_value));
	InitializeCriticalSection(&l->lock);
	return l;
}

void ssl_lock_dyn_callback(int mode, CRYPTO_dynlock_value* l, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
		EnterCriticalSection(&l->lock);
	else
		LeaveCriticalSection(&l->lock);
}

void ssl_lock_dyn_destroy_callback(CRYPTO_dynlock_value* l, const char *file, int line)
{
	DeleteCriticalSection(&l->lock);
	free(l);
}

#endif



void ssl_init()
{
#if OPENSSL_VERSION_NUMBER == 0x1000105fL

	

	number_of_locks = CRYPTO_num_locks();
	if (number_of_locks > 0)
	{
		ssl_locks = (ssl_lock*)malloc(number_of_locks * sizeof(ssl_lock));
		for (int n = 0; n < number_of_locks; ++n)
			InitializeCriticalSection(&ssl_locks[n]);
	}


#ifdef _DEBUG
	CRYPTO_malloc_debug_init();
	CRYPTO_dbg_set_options(V_CRYPTO_MDEBUG_ALL);
	CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
#endif

	CRYPTO_set_locking_callback(&ssl_lock_callback);
	CRYPTO_set_dynlock_create_callback(&ssl_lock_dyn_create_callback);
	CRYPTO_set_dynlock_lock_callback(&ssl_lock_dyn_callback);
	CRYPTO_set_dynlock_destroy_callback(&ssl_lock_dyn_destroy_callback);

#endif

	//ERR_load_BIO_strings();
	//OpenSSL_add_all_algorithms();

	SSL_load_error_strings();
	SSL_library_init();


}