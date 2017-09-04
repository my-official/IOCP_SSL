#include "base.h"
#include "Transport.h"

volatile bool g_ServerQuit = false;
volatile bool g_ClientQuit = false;

LPFN_ACCEPTEX g_lpfnAcceptEx = nullptr;
LPFN_GETACCEPTEXSOCKADDRS g_lpfnGetAcceptExSockaddrs = nullptr;

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

DWORD IOCPBase::sm_NumRecvThreads = 4;



void IOCPBase::Recv()
{
	ULONG_PTR lpContext = NULL;
	OVERLAPPED       *pOverlapped = NULL;
	TCPTransport   *pClientContext = NULL;
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
			&lpContext,
			&pOverlapped,
			INFINITE);

		if (bReturn)
		{
			if (reinterpret_cast<IOCPBase*>(lpContext) == this)
			{
				//Quiting
				return;
			}

			assert(pOverlapped);

			auto completable = reinterpret_cast<Socketable*>(lpContext);

			//DWORD flag = 0;

			//if ( WSAGetOverlappedResult(completable->m_Socket, pOverlapped, &dwBytesTransfered, FALSE, &flag) != TRUE)
			//{
			//	throw EXCEPTION( WSAException() );
			//}

			/*if (flag & MSG_PARTIAL)
			{
				++flag;
			}*/

			//auto tcpTransport = dynamic_cast<TCPTransport*>(lpContext);
			//
	/*		if (dwBytesTransfered > 60000)
			{
				completable->Complition(dwBytesTransfered, pOverlapped);
				return;
			}*/

			//DWORD size = std::min<DWORD>(dwBytesTransfered,sizeof(g_NETDATA) * 5);

			completable->Complition(dwBytesTransfered, pOverlapped);
		}
		else
		{
			if (pOverlapped)
			{
				WriteToConsole("%s: transport error", m_Prefix.c_str());
			//	RemoveTransport((TCPTransport*)lpContext);
			}
			else
			{
				throw EXCEPTION(SystemException());
			}
		}
	}
}


void IOCPBase::AssociateWithIOCP(Socketable* socketable)
{
//	assert(!socketable->m_IsUDPTransport || m_UDPDomain == socketable);		

	HANDLE hTemp = CreateIoCompletionPort((HANDLE)socketable->m_Socket, m_hIOCompletionPort, (ULONG_PTR)socketable, 0);

	if (NULL == hTemp)
	{
		WriteToConsole("AssociateWithIOCP GetLastError == %i", GetLastError());
		throw EXCEPTION(SystemException());
	}
}


void IOCPBase::AddTransport(Transport* transport)
{
	EnterCriticalSection(&m_Socket2TransportsCS);

	m_Socket2Transports.insert(make_pair(transport->m_Socket, transport));

	LeaveCriticalSection(&m_Socket2TransportsCS);
}


void IOCPBase::RemoveTransport(Transport* client)
{	
	EnterCriticalSection(&m_Socket2TransportsCS);

	auto it = find_if(m_Socket2Transports.begin(), m_Socket2Transports.end(), [client](const pair<SOCKET, Transport* >& e)->bool
	{
		return e.second == client;
	});

	if (it != m_Socket2Transports.end())
		//auto it = m_Socket2Clients.find(transport->m_ClientSocket);
		//assert(it != m_Socket2Clients.end());
	{
		if (it->second->m_IsUDPTransport)
		{
			UDPTransport* udpTransport = static_cast<UDPTransport*>(it->second);
			m_UDPDomain->Remove(udpTransport);
		}

		delete it->second;
		m_Socket2Transports.erase(it);
	}

	LeaveCriticalSection(&m_Socket2TransportsCS);	
}

void IOCPBase::CreateIOCP()
{
	m_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	if (NULL == m_hIOCompletionPort)
	{
		m_hIOCompletionPort = INVALID_HANDLE_VALUE;
		throw EXCEPTION(SystemException());
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
			throw EXCEPTION(SystemException());
		}

		m_RecvThreads.push_back(thread);
	}
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
		
		assert(result);		
		m_hIOCompletionPort = INVALID_HANDLE_VALUE;
	}
}

void IOCPBase::PrintTotalRecived()
{
	int id = 0;
	decltype(m_Socket2Transports) socket2Transports;

	EnterCriticalSection(&m_Socket2TransportsCS);

	socket2Transports = m_Socket2Transports;

	//m_Socket2Transports.swap(socket2Transports);
	
	LeaveCriticalSection(&m_Socket2TransportsCS);

	string text;
	text.reserve(100);

	for (auto& s2t : socket2Transports)
	{		
		Transport* transport = s2t.second;
		
		static const int deltaSeconds = TimerThread::DelayMilliseconds / 1000;
		
		static const string dims[] = { "B", "KB", "MB", "GB" };
		DWORD speed = transport->m_TotalRecived / deltaSeconds;
		
		int c_dim = 0;

		for (int size_dim = 2; speed >= 1024 && c_dim < size_dim; c_dim++)
		{
			speed /= 1024;
		}

		string dim = dims[c_dim] + "/sec";

		string transportType;
		if (transport->m_IsUDPTransport)
		{
			transportType = "UDP";
		}
		else
		{
			if (dynamic_cast<SSLTransport*>(transport))
				transportType = "SSL"; 
			else
				transportType = "TCP";
		}
			
						
		text = m_Prefix + " " + transportType + " transport " + to_string(id++) + " " + to_string(speed) + " " + dim + " NumErrors " + to_string(transport->m_ErrorCounter);

		WriteToConsole( &text[0] );
		transport->m_TotalRecived = 0;
		transport->m_ErrorCounter = 0;
	}	

	WriteToConsole(" ");
}

void IOCPBase::StartUDP()
{
	m_UDPDomain = new UDPDomain(this);
	m_UDPDomain->Bind(SERVER_PORT);
	m_UDPDomain->StartReceiving();
}

TCPTransport* IOCPBase::NewTCPTransport(SOCKET socket /*= INVALID_SOCKET*/)
{
	return new TCPTransport(this, socket);
}

UDPTransport* IOCPBase::NewUDPTransport(UDPDomain* udpDomain)
{
	return new UDPTransport(this, udpDomain);
}


SSLTransport* IOCPBase::NewSSLTransport(SOCKET socket /*= INVALID_SOCKET*/)
{
	return new SSLTransport(this, socket);
}

IOCPBase::IOCPBase() : m_hIOCompletionPort(INVALID_HANDLE_VALUE),
						m_TCPServer(nullptr),
						m_SSLServer(nullptr),
						m_UDPDomain(nullptr),
						m_TimerThread(this)
{
	ZeroMemory(&m_Socket2TransportsCS, sizeof(m_Socket2TransportsCS));
	InitializeCriticalSection(&m_Socket2TransportsCS);

	CreateIOCP();
	CreateRecvThreadPool();
	Thread::RunThread(m_TimerThread);
}

IOCPBase::~IOCPBase()
{
	m_TimerThread.Quit();
	DestroyRecvThreadPool();	

	for (auto& transport : m_Socket2Transports)
	{
		delete transport.second;
	}
	m_Socket2Transports.clear();
	
	DestroyIOCP();


	DeleteCriticalSection(&m_Socket2TransportsCS);	
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








HLTransportData* HL_IOCP::GetHLTransportData(Transport* transport)
{
	
	if ( dynamic_cast< HLTransport<UDPTransport>* >(transport) )
		return static_cast<HLTransportData*>( static_cast< HLTransport<UDPTransport>* >(transport) );
	else
	{
		if (dynamic_cast< HLTransport<SSLTransport>* >(transport))
		{
			return static_cast<HLTransportData*>(static_cast<HLTransport<SSLTransport>*>(transport));
		}
		else
		{
			assert(dynamic_cast< HLTransport<TCPTransport>* >(transport) );
			return static_cast<HLTransportData*>(static_cast<HLTransport<TCPTransport>*>(transport));
		}		
	}
}

TCPTransport* HL_IOCP::NewTCPTransport(SOCKET socket /*= INVALID_SOCKET*/)
{
	return new HLTransport<TCPTransport>(this, socket);
}

UDPTransport* HL_IOCP::NewUDPTransport(UDPDomain* udpDomain)
{
	return new HLTransport<UDPTransport>(this, udpDomain);
}

SSLTransport* HL_IOCP::NewSSLTransport(SOCKET socket /*= INVALID_SOCKET*/)
{
	return new HLTransport<SSLTransport>(this, socket);
}

