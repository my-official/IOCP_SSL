#include "base.h"

volatile bool g_ServerQuit = false;
volatile bool g_ClientQuit = false;

uint32_t g_NETDATA[g_NETDATA_SIZE] = { 0 };

CRITICAL_SECTION g_ConsoleCS;

void WriteToConsole(char *szFormat, ...)
{
	EnterCriticalSection(&g_ConsoleCS);

	va_list args;
	va_start(args, szFormat);

	vprintf(szFormat, args);

	va_end(args);

	printf("\n");

	LeaveCriticalSection(&g_ConsoleCS);
}

DWORD IOCPBase::sm_NumRecvThreads = 1;


void IOCPBase::Recv()
{
	void *lpContext = NULL;
	OVERLAPPED       *pOverlapped = NULL;
	Transport   *pClientContext = NULL;
	DWORD            dwBytesTransfered = 0;
	int nBytesRecv = 0;
	int nBytesSent = 0;
	DWORD             dwBytes = 0, dwFlags = 0;

	//Worker thread will be around to process requests, until a Shutdown event is not Signaled.
	while (true)
	{
		BOOL bReturn = GetQueuedCompletionStatus(
			m_hIOCompletionPort,
			&dwBytesTransfered,
			(LPDWORD)&lpContext,
			&pOverlapped,
			INFINITE);


		if (bReturn)
		{			
			if (lpContext == (IOCPBase*)this)
			{
				//Quiting
				return;
			}

			ASSERT(pOverlapped);

			Transport* transport = (Transport*)lpContext;

			if (dwBytesTransfered > 0)
			{
			//	transport->m_CurrRecived += dwBytesTransfered;
				if (!OnRecived(transport, dwBytesTransfered))
				{
					RemoveTransport(transport);					
				}				
			}
			else
			{				
				WriteToConsole("%s: transport shutdown", m_Prefix.c_str());
				RemoveTransport(transport);
			}

		}
		else
		{
			if (pOverlapped)
			{
				WriteToConsole("%s: transport error", m_Prefix.c_str());
				RemoveTransport((Transport*)lpContext);
			}
			else
			{
				throw SYS_EXCEPTION;
			}
		}
	}
}





void IOCPBase::RemoveTransport(Transport* transport)
{
	EnterCriticalSection(&m_Socket2ClientsCS);	

	auto it = find_if(m_Socket2Clients.begin(), m_Socket2Clients.end(), [transport](const pair<SOCKET, Transport* >& e)->bool
	{
		return e.second == transport;
	});

	if (it != m_Socket2Clients.end())
		//auto it = m_Socket2Clients.find(transport->m_ClientSocket);
		//ASSERT(it != m_Socket2Clients.end());
	{
		delete it->second;
		m_Socket2Clients.erase(it);
	}

	LeaveCriticalSection(&m_Socket2ClientsCS);
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
	//Create worker threads
	for (DWORD c = 0; c < sm_NumRecvThreads; c++)
	{
		RecvThread* thread = new RecvThread(this);
		
		if (!Thread::RunThread(*thread))
		{
			//остановить потоки
			m_RecvThreads.clear();
			throw SYS_EXCEPTION;
		}

		m_RecvThreads.push_back(thread);
	}
}

void IOCPBase::AssociateWithIOCP(Transport* transport)
{
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)transport->m_ClientSocket, m_hIOCompletionPort, (DWORD)transport, 0);
	

	if (NULL == hTemp)
	{
		WriteToConsole("AssociateWithIOCP GetLastError == %i", GetLastError());
		throw SYS_EXCEPTION;
	}

	EnterCriticalSection( &m_Socket2ClientsCS ); 

	m_Socket2Clients.insert(make_pair(transport->m_ClientSocket, transport));

	LeaveCriticalSection( &m_Socket2ClientsCS );
}

void IOCPBase::DestroyRecvThreadPool()
{
	Thread::QuitSome(m_RecvThreads.begin(), m_RecvThreads.end() );
	for (auto thread : m_RecvThreads)
	{
		delete thread;
	}

	m_RecvThreads.clear();
}

void IOCPBase::DestroyIOCP()
{	
	if (m_hIOCompletionPort != INVALID_HANDLE_VALUE)
	{
		BOOL result = CloseHandle(m_hIOCompletionPort);
		
		ASSERT(result);		
		m_hIOCompletionPort = INVALID_HANDLE_VALUE;
	}
}

void IOCPBase::PrintTotalRecived()
{
	int id = 0;
	decltype(m_Socket2Clients) socket2Clients;

	EnterCriticalSection(&m_Socket2ClientsCS);

	socket2Clients = m_Socket2Clients;

	LeaveCriticalSection(&m_Socket2ClientsCS);


	for (auto& s2t : socket2Clients)
	{		
		Transport* transport = s2t.second;
		
		WriteToConsole("%s transport %i speed %u KB NumErrors %u", m_Prefix.c_str(), id++, (transport->m_TotalRecived  / (TimerThread::DelayMilliseconds / 1000) ) / (1024 ), transport->m_ErrorCounter );
		transport->m_TotalRecived = 0;
		transport->m_ErrorCounter = 0;
	}
}

Transport* IOCPBase::NewTransport(SOCKET socket)
{
	return new Transport(this, socket);
}

UDPTransport* IOCPBase::NewUDPTransport(SOCKET socket)
{
	return new UDPTransport(this, socket);	
}

IOCPBase::IOCPBase() : m_hIOCompletionPort(INVALID_HANDLE_VALUE), m_TimerThread(this)
{
	ZeroMemory(&m_Socket2ClientsCS, sizeof(m_Socket2ClientsCS));
	InitializeCriticalSection(&m_Socket2ClientsCS);

	CreateIOCP();
	CreateRecvThreadPool();
	Thread::RunThread(m_TimerThread);
}

IOCPBase::~IOCPBase()
{
	m_TimerThread.Quit();
	DestroyRecvThreadPool();
	

	for (auto& transport : m_Socket2Clients)
	{
		delete transport.second;
	}
	m_Socket2Clients.clear();

	
	DestroyIOCP();


	DeleteCriticalSection(&m_Socket2ClientsCS);
}


Transport::Transport(IOCPBase* iocpBase, SOCKET clientSocket) : m_IOCPBaseParent(iocpBase),
																m_ClientSocket(clientSocket),																
																m_TotalRecived(0),
																m_CurrRecived(0),
																m_CurrProcessed(0),
																m_ErrorCounter(0)
{
	u_long value = 1;
	
	ioctlsocket(m_ClientSocket, FIONBIO, &value);

	//ZeroMemory(&m_SendOverlapped, sizeof(m_SendOverlapped));
	//m_SendOverlapped.hEvent = WSACreateEvent();
	//if (m_SendOverlapped.hEvent == WSA_INVALID_EVENT)
	//	throw SYS_EXCEPTION;



	m_RecvBuffer.resize(sizeof(g_NETDATA) * 5);

	ZeroMemory(&m_ReciveOverlapped, sizeof(m_ReciveOverlapped));
	m_ReciveOverlapped.hEvent = WSACreateEvent();
	if (m_ReciveOverlapped.hEvent == WSA_INVALID_EVENT)
		throw SYS_EXCEPTION;
}


Transport::~Transport()
{
	//if (m_SendOverlapped.hEvent != 0)
	//{
	//	WSACloseEvent(m_SendOverlapped.hEvent);
	//}

	if (m_ReciveOverlapped.hEvent != 0)
	{
		WSACloseEvent(m_ReciveOverlapped.hEvent);
	}
	
	if (m_ClientSocket != INVALID_SOCKET)
	{
		closesocket(m_ClientSocket);
		m_ClientSocket = INVALID_SOCKET;
	}
}


bool Transport::Send(char* data, ULONG size)
{
	//DWORD dwBytes;

	//WSABUF buffer;
	//buffer.buf = (CHAR*)data;
	//buffer.len = size;


	//auto nBytesSent = WSASend(m_ClientSocket, &buffer, 1, &dwBytes, 0, &m_SendOverlapped, NULL);

	//if ((SOCKET_ERROR != nBytesSent) || (WSA_IO_PENDING == WSAGetLastError()))
	//	return true;
	//else
	//	return false;

	int result = send(m_ClientSocket, data, size, 0);

	if (result != SOCKET_ERROR)
		return true;
	else
	{		
		return (WSAGetLastError() == WSAEWOULDBLOCK);
	}
}

bool Transport::Recive()
{
	DWORD dwBytes = 0;
	DWORD flag = 0;

	WSABUF buffer;
	buffer.buf = (CHAR*)m_RecvBuffer.data() + m_CurrRecived;
	buffer.len = m_RecvBuffer.size() - m_CurrRecived;

	ASSERT(buffer.len > 0);

	auto result = WSARecv(m_ClientSocket, &buffer, 1, NULL, &flag, &m_ReciveOverlapped, NULL);

	return ((SOCKET_ERROR != result) || (WSA_IO_PENDING == WSAGetLastError()));
}


IOCPBase::RecvThread::RecvThread(IOCPBase* parent) : m_Parent(parent)
{

}

void IOCPBase::RecvThread::ThreadMain()
{
	m_Parent->Recv();
}

void IOCPBase::RecvThread::SendQuit()
{
	PostQueuedCompletionStatus(m_Parent->m_hIOCompletionPort, 0, (ULONG_PTR)m_Parent, NULL);
}

const int IOCPBase::TimerThread::DelayMilliseconds = 2000;

IOCPBase::TimerThread::TimerThread(IOCPBase* parent) : m_Parent(parent), m_Quit(false)
{

}

void IOCPBase::TimerThread::ThreadMain()
{
	while (!m_Quit)
	{
		Sleep(DelayMilliseconds);
		m_Parent->PrintTotalRecived();
	}
}

void IOCPBase::TimerThread::SendQuit()
{
	m_Quit = true;
}

bool UDPTransport::Send(char* data, ULONG size)
{
	int result = sendto(m_ClientSocket, data, size, 0, &m_Address, sizeof(m_Address));

	if (result != SOCKET_ERROR)
		return true;
	else
	{
		return (WSAGetLastError() == WSAEWOULDBLOCK);
	}
}

bool UDPTransport::Recive()
{
	DWORD dwBytes = 0;
	DWORD flag = 0;

	WSABUF buffer;
	buffer.buf = (CHAR*)m_RecvBuffer.data() + m_CurrRecived;
	buffer.len = m_RecvBuffer.size() - m_CurrRecived;

	ASSERT(buffer.len > 0);

	auto result = WSARecvFrom(m_ClientSocket, &buffer, 1, NULL, &flag, &m_Address, &m_AddressLen , &m_ReciveOverlapped);

	return ((SOCKET_ERROR != result) || (WSA_IO_PENDING == WSAGetLastError()));
}

UDPTransport::UDPTransport(IOCPBase* iocpBase, SOCKET clientSocket) : Transport(iocpBase, clientSocket)
{

}