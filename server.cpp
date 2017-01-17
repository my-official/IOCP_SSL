#include "server.h"

Server server;

unsigned __stdcall ServerMain(void*)
{
	cout << "ServerMain" << endl;
	
	server.ThreadMain();
	return 0;
}




void Server::ThreadMain()
{
	CreateIOCP();
	CreateRecvThreadPool();	
	CreateListenSocket();

	while (!g_ServerQuit)
	{
		Acceptor();
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

	auto client = new Transport(this, clientSocket);
	
	AssociateWithIOCP(client);

	m_Socket2Clients.insert(make_pair(client->m_ClientSocket, client));

}




void Server::AssociateWithIOCP(Server::Transport * client)
{
	//Associate the socket with IOCP
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)client->m_ClientSocket, m_hIOCompletionPort, (DWORD)client, 0);

	if (NULL == hTemp)
	{
		throw SYS_EXCEPTION;		
	}

}

void Server::CreateListenSocket()
{
	struct sockaddr_in serverAddress;

	//Overlapped I/O follows the model established in Windows and can be performed only on 
	//sockets created through the WSASocket function 
	m_ListenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, 0);

	if (INVALID_SOCKET == m_ListenSocket)
	{
		printf("\nError occurred while opening socket: %d.", WSAGetLastError());
		return;
	}

	//Cleanup and Init with 0 the ServerAddress
	ZeroMemory((char *)&serverAddress, sizeof(serverAddress));

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY; //WinSock will supply address
	serverAddress.sin_port = htons(SERVER_PORT);    //comes from commandline
													//Assign local address and port number
	if (SOCKET_ERROR == bind(m_ListenSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)))
	{
		closesocket(m_ListenSocket);
		printf("\nError occurred while binding.");

	}
	else
	{
		printf("\nbind() successful.");
	}

	//Make the socket a listening socket
	if (SOCKET_ERROR == listen(m_ListenSocket, SOMAXCONN))
	{
		closesocket(m_ListenSocket);
		printf("\nError occurred while listening.");
		return;
	}
	else
	{
		printf("\nlisten() successful.");
	}
}





void Server::RemoveClient(Transport* client)
{	
	m_Socket2Clients.erase(client->m_ClientSocket);
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



void Server::RecvThread()
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
			assert(pOverlapped);

			if (lpContext == (void*)&server)
			{
				//Quiting
				return;
			}

			Transport* client = (Transport*)lpContext;

			if (dwBytesTransfered > 0)
			{
				if (!client->OnRecived(dwBytesTransfered))
				{

					RemoveClient(client);
				}
			}
			else
			{
				WriteToConsole("client shutdown");
				RemoveClient(client);
			}

		}
		else
		{

			if (pOverlapped)
			{
				WriteToConsole("client error");
				RemoveClient((Transport*)lpContext);
			}
			else
			{
				throw SYS_EXCEPTION;
			}
		}
	}
}
	
Server::Transport::Transport(Server* server, SOCKET clientSocket) : m_Server(server), m_ClientSocket(clientSocket)
{
	ZeroMemory(&m_SendOverlapped, sizeof(OVERLAPPED));
	ZeroMemory(&m_ReciveOverlapped, sizeof(OVERLAPPED));	
	m_RecvBuffer.resize(g_NETDATA_SIZE);

	static int id = 0;
	m_Id = id++;
}

bool Server::Transport::Send(DWORD numOfInts)
{
	DWORD dwBytes;

	WSABUF buffer;
	buffer.buf = (CHAR*)g_NETDATA;
	buffer.len = numOfInts;

	auto nBytesSent = WSASend(m_ClientSocket, &buffer, 1,&dwBytes, 0, &m_SendOverlapped, NULL);

	if ((SOCKET_ERROR != nBytesSent) || (WSA_IO_PENDING == WSAGetLastError()))
		return true;
	else
		return false;
}

bool Server::Transport::Recive()
{
	DWORD dwBytes;

	WSABUF buffer;
	buffer.buf = (CHAR*)m_RecvBuffer.data();
	buffer.len = m_RecvBuffer.size();

	auto nBytesRecv = WSARecv(m_ClientSocket, &buffer, 1, &dwBytes, 0, &m_ReciveOverlapped, NULL);

	if ((SOCKET_ERROR != nBytesRecv) || (WSA_IO_PENDING == WSAGetLastError()))
		return true;
	else
		return false;
}

bool Server::Transport::OnRecived(DWORD dwBytesTransfered)
{
	m_TotalRecived += dwBytesTransfered;

	if (m_TotalRecived < sizeof(DWORD))
	{
		return true;
	}
	else
	{
		if (!Send(*(DWORD*)m_RecvBuffer.data()))
		{
			return false;
		}

		return Recive();
	}
}

Server::Transport::~Transport()
{
	if (m_ClientSocket != INVALID_SOCKET)
	{
		closesocket(m_ClientSocket);
		m_ClientSocket = INVALID_SOCKET;
	}
}
