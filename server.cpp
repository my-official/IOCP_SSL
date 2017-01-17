#include "server.h"





void Server::ThreadMain()
{
	StartTCPAcceptor();

	StartUDPSocket();
	//Acceptor();
	

	while (!m_Quit)
	{		
		//Acceptor();
		Sleep(100);
	};
}


void Server::Acceptor()
{
	sockaddr_in ClientAddress;
	int nClientLength = sizeof(ClientAddress);

	//Accept remote connection attempt from the client
	SOCKET clientSocket = accept(m_ListenSocket, (sockaddr*)&ClientAddress, &nClientLength);

	if (INVALID_SOCKET == clientSocket)
	{
		WriteToConsole("\nError occurred while accepting socket: %ld.", WSAGetLastError());
	}

	auto transport = NewTransport(clientSocket);
	
	AssociateWithIOCP(transport);

	if (!transport->Recive())
	{
		throw SYS_EXCEPTION;
	}
}



void Server::StartTCPAcceptor()
{
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)m_ListenSocket, m_hIOCompletionPort, (ULONG_PTR)&m_ListenSocket, 0);


	if (NULL == hTemp)
	{
		WriteToConsole("AssociateWithIOCP GetLastError == %i", GetLastError());
		throw SYS_EXCEPTION;
	}
	
	
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	DWORD dwBytes = 0;

	int iResult = WSAIoctl(m_ListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx, sizeof(GuidAcceptEx),
		&m_lpfnAcceptEx, sizeof(m_lpfnAcceptEx),
		&dwBytes, NULL, NULL);

	if (iResult == SOCKET_ERROR)
	{
		throw SYS_EXCEPTION;
	}

	//ASSERT(dwBytes == 0);
		

	ResumeTCPAccept();
	



	
}

void Server::StartUDPSocket()
{
	SOCKET m_UDPSocket = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == m_UDPSocket)
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
	
	if (SOCKET_ERROR == bind(m_UDPSocket, (struct sockaddr *) resolvedServerAddress, sizeof(resolvedServerAddress)))
		throw SYS_EXCEPTION;
	
	freeaddrinfo(resolvedServerAddress);	
	

	UDPTransport* transport = NewUDPTransport(m_UDPSocket);
	AssociateWithIOCP(transport);
	
	if (!transport->Recive())
	{
		throw SYS_EXCEPTION;
	}	
}

void Server::RemoveTransport(Transport* client)
{
	if ((SOCKET*)client == &m_ListenSocket)
	{
		//
		WriteToConsole("m_ListenSocket removed");
		DeleteListenSocket();
	}
	else
	{
		IOCPBase::RemoveTransport(client);
	}
}

void Server::CreateListenSocket()
{
	//Overlapped I/O follows the model established in Windows and can be performed only on 
	//sockets created through the WSASocket function 
	m_ListenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == m_ListenSocket)
	{
		throw SYS_EXCEPTION;
	}

	struct sockaddr_in serverAddress;
	//Cleanup and Init with 0 the ServerAddress
	ZeroMemory((char *)&serverAddress, sizeof(serverAddress));

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY; //WinSock will supply address
	serverAddress.sin_port = htons( atoi(SERVER_PORT) );    //comes from commandline
													//Assign local address and port number
	if (SOCKET_ERROR == bind(m_ListenSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)))
			throw SYS_EXCEPTION;
	
	WriteToConsole("\nbind() successful.");
	

	//Make the socket a listening socket
	if (SOCKET_ERROR == listen(m_ListenSocket, SOMAXCONN))
		throw SYS_EXCEPTION;
	
	WriteToConsole("\nlisten() successful.");
	
}



void Server::DeleteListenSocket()
{
	if (m_ListenSocket != INVALID_SOCKET)
	{
		closesocket(m_ListenSocket);
		m_ListenSocket = INVALID_SOCKET;
	}
}

bool Server::OnRecived(Transport* transport, DWORD dwBytesTransfered)
{
	if (transport == (Transport*)&m_ListenSocket)
	{
		int iResult = 0;

		iResult = setsockopt(transport->m_ClientSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
			(char *)&m_ListenSocket, sizeof(m_ListenSocket));

		transport = m_AcceptedTransport;

		ResumeTCPAccept();
	}

	
	transport->m_CurrRecived += dwBytesTransfered;

	transport->m_TotalRecived += dwBytesTransfered;

	uint32_t* pbuffer = (uint32_t*)transport->m_RecvBuffer.data();

	while (transport->m_CurrRecived >= sizeof(uint32_t))
	{
		uint32_t numOfInts = *pbuffer;

		if (!transport->Send((char*)g_NETDATA, numOfInts * sizeof(uint32_t)))
		{
			return false;
		}


		transport->m_CurrRecived -= sizeof(uint32_t);
		++pbuffer;		
	}


	if (!transport->Recive())
	{
		WriteToConsole("%s: transport->Recive error", m_Prefix.c_str());
		return false;
	}

	return true;
}

Server::Server() : m_Quit(false), m_ListenSocket(INVALID_SOCKET), m_lpfnAcceptEx(NULL), m_AcceptedTransport(NULL)
{
	m_Prefix = "Server";
	CreateListenSocket();
}

Server::~Server()
{
	DestroyRecvThreadPool();
	DeleteListenSocket();
}

void Server::ResumeTCPAccept()
{
	SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (socket == INVALID_SOCKET)
	{
		throw SYS_EXCEPTION;
	}

	auto transport = NewTransport(socket);

	AssociateWithIOCP(transport);


	BOOL result = m_lpfnAcceptEx(m_ListenSocket, socket, (PVOID)transport->m_RecvBuffer.data(), transport->m_RecvBuffer.size() - ((sizeof(sockaddr_in) + 16) * 2),
		sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
		NULL, &transport->m_ReciveOverlapped);


	if (result == FALSE && WSAGetLastError() != WSA_IO_PENDING)
	{
		WriteToConsole("AcceptEx Error! WSAGetLastError %i", WSAGetLastError());
		throw SYS_EXCEPTION;
	}

	m_AcceptedTransport = transport;
}

////This thread will look for accept event
//DWORD WINAPI AcceptThread(LPVOID lParam)
//{
//	SOCKET ListenSocket = (SOCKET)lParam;
//
//	WSANETWORKEVENTS WSAEvents;
//
//	//Accept thread will be around to look for accept event, until a Shutdown event is not Signaled.
//	while (WAIT_OBJECT_0 != WaitForSingleObject(g_hShutdownEvent, 0))
//	{
//		if (WSA_WAIT_TIMEOUT != WSAWaitForMultipleEvents(1, &g_hAcceptEvent, FALSE, WAIT_TIMEOUT_INTERVAL, FALSE))
//		{
//			WSAEnumNetworkEvents(ListenSocket, g_hAcceptEvent, &WSAEvents);
//			if ((WSAEvents.lNetworkEvents & FD_ACCEPT) && (0 == WSAEvents.iErrorCode[FD_ACCEPT_BIT]))
//			{
//				//Process it
//				AcceptConnection(ListenSocket);
//			}
//		}
//	}
//
//	return 0;
//}


