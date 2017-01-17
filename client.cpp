#include "client.h"

const uint32_t numUInt32ForRecv = 15000;

void Client::ThreadMain()
{
	for (int c = 0, size = 2; c < size; c++)
	{
		SOCKET socket = NewUDPConnection();
		Transport* transport = NewTransport(socket);
		AssociateWithIOCP(transport);	

		ASSERT(numUInt32ForRecv <= g_NETDATA_SIZE);
		transport->Recive();
		transport->Send((char*)&numUInt32ForRecv, sizeof(numUInt32ForRecv));
	}

	while (!m_Quit)
	{
		Sleep(5000);


		//for (int c = 0, size = 2; c < size; c++)
		//{
		//	SOCKET socket = NewTCPConnection();
		//	Transport* transport = NewTransport(socket);
		//	AssociateWithIOCP(transport);

		//	ASSERT(numUInt32ForRecv <= g_NETDATA_SIZE);
		//	transport->Recive();
		//	transport->Send((char*)&numUInt32ForRecv, sizeof(numUInt32ForRecv));
		//}

		//decltype(m_ParseQueue) parseQueue;

		//EnterCriticalSection(&m_ParseQueueCS);

		//if (!m_ParseQueue.empty())
		//{
		//	m_ParseQueue.swap(parseQueue);			
		//	
		//	LeaveCriticalSection(&m_ParseQueueCS);
		//	
		//	while (!parseQueue.empty())
		//	{
		//		Transport* transport = parseQueue.back();
		//		parseQueue.pop();

		//		ProcessTransport(transport);
		//	}
		//}
		//else
		//{
		//	LeaveCriticalSection(&m_ParseQueueCS);
		//	auto result = WaitForSingleObject(m_hSignal, 200);

		//	if (result != WAIT_OBJECT_0 && result != WAIT_TIMEOUT)
		//	{
		//		throw SYS_EXCEPTION;
		//	}
		//}		
	}

}

SOCKET Client::NewTCPConnection()
{	
	SOCKET socket = INVALID_SOCKET;

	struct addrinfo *resolvedServerAddress = NULL,
		*ptr = NULL,
		hints;


	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the TCPSERVER address and port
	if (getaddrinfo("127.0.0.1", SERVER_PORT, &hints, &resolvedServerAddress))
		return false;



	// Attempt to connect to an address until one succeeds
	for (ptr = resolvedServerAddress; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to TCPSERVER		
		//socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

		if (INVALID_SOCKET == socket)
		{
			throw SYS_EXCEPTION;		
		}

		// Connect to TCPSERVER.
		if (connect(socket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR)
		{
			WriteToConsole("connect Error. %i", WSAGetLastError());
			closesocket(socket);
			socket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(resolvedServerAddress);

	if (socket == INVALID_SOCKET)
		throw SYS_EXCEPTION;


	return socket;
}


SOCKET Client::NewUDPConnection()
{
	SOCKET socket = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == socket)
	{
		throw SYS_EXCEPTION;
	}

	struct addrinfo serverAddress;
	struct addrinfo *resolvedServerAddress;
	//Cleanup and Init with 0 the ServerAddress
	ZeroMemory((char *)&serverAddress, sizeof(serverAddress));
	ZeroMemory((char *)&resolvedServerAddress, sizeof(resolvedServerAddress));

	serverAddress.ai_family = AF_INET;
	serverAddress.ai_socktype = SOCK_DGRAM;
	serverAddress.ai_protocol = IPPROTO_UDP;

	// Resolve the TCPSERVER address and port
	if (getaddrinfo("127.0.0.1", SERVER_PORT, &serverAddress, &resolvedServerAddress))
		throw SYS_EXCEPTION;

	// Connect to TCPSERVER.
	if (connect(socket, resolvedServerAddress->ai_addr, (int)resolvedServerAddress->ai_addrlen) == SOCKET_ERROR)
		throw SYS_EXCEPTION;

	freeaddrinfo(resolvedServerAddress);

	return socket;
}

bool Client::OnRecived(Transport* transport, DWORD dwBytesTransfered)
{
	transport->m_TotalRecived += dwBytesTransfered;	

	transport->m_CurrRecived += dwBytesTransfered;



	if (transport->m_CurrRecived >= numUInt32ForRecv * sizeof(uint32_t))
	{		
		ASSERT(transport->m_CurrRecived == numUInt32ForRecv * sizeof(uint32_t));
		
		ProcessTransport(transport);

		//AddTransportForProcess(transport);		

		transport->m_CurrRecived = 0;
	}


	if (!transport->Recive())
	{
		WriteToConsole("%s: transport->Recive error", m_Prefix.c_str());
		return false;
	}

	return true;
}

Client::Client() : m_Quit(false), m_hSignal(NULL)
{
	m_Prefix = "Client";

	m_hSignal = CreateEvent(NULL, FALSE, FALSE, "ClientSiganl");

	if (m_hSignal == NULL)
	{
		throw SYS_EXCEPTION;
	}

	ZeroMemory(&m_ParseQueueCS, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(&m_ParseQueueCS);
}

Client::~Client()
{	
	DestroyRecvThreadPool();	
	CloseHandle(m_hSignal);

	DeleteCriticalSection(&m_ParseQueueCS);
	ZeroMemory(&m_ParseQueueCS, sizeof(CRITICAL_SECTION));
}

void Client::Validate(Transport* transport)
{
	uint32_t* buffer = (uint32_t*)transport->m_RecvBuffer.data();
	uint32_t control_value = buffer[0];
	for (uint32_t c = 0, size = transport->m_CurrRecived / sizeof(uint32_t); c < size; c++)
	{
		transport->m_ErrorCounter += (buffer[c] != control_value++);
	}
}

void Client::FastValidate(Transport* transport)
{
	uint32_t* buffer = (uint32_t*)transport->m_RecvBuffer.data();

			
	//if ( transport->m_CurrRecived < (numUInt32ForRecv * sizeof(uint32_t)) )
	//{
	//	transport->m_ErrorCounter += 1000000;

	//}
	//else
	{
		transport->m_ErrorCounter += buffer[0] != 0;
		transport->m_ErrorCounter += (buffer[numUInt32ForRecv - 1] != (numUInt32ForRecv - 1));
	}
	
	
}

void Client::AddTransportForProcess(Transport* transport)
{
	EnterCriticalSection(&m_ParseQueueCS);

	m_ParseQueue.push(transport);

	LeaveCriticalSection(&m_ParseQueueCS);

	SetEvent(m_hSignal);
	
}

void Client::ProcessTransport(Transport* transport)
{
	FastValidate(transport);
	//Validate(transport);
		
	//transport->m_CurrProcessed += numUInt32ForRecv * sizeof(numUInt32ForRecv);	
	ASSERT(numUInt32ForRecv <= g_NETDATA_SIZE);	
	//transport->Send((char*)&numUInt32ForRecv, sizeof(numUInt32ForRecv));
	
	//ASSERT(transport->m_CurrRecived == 0);

	//if (!transport->Recive())
	//{
	//	throw SYS_EXCEPTION;
	//	WriteToConsole("%s: transport->Recive error", m_Prefix.c_str());
	//	RemoveTransport(transport);
	//}


	if (!transport->Send((char*)&numUInt32ForRecv, sizeof(numUInt32ForRecv)))
	{
	//	throw SYS_EXCEPTION;
		WriteToConsole("%s: transport->Send error", m_Prefix.c_str());		
		RemoveTransport(transport);
	}
}
