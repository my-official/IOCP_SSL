#include "server.h"
#include "Transport.h"




Server::Server() : m_Quit(false)
{
	m_Prefix = "Server";
}




void Server::ThreadMain()
{	
	m_TCPServer = new TCPServer(this, atoi(SERVER_PORT));
	m_TCPServer->StartAcceptor();
	m_SSLServer = new SSLServer(this, atoi(SERVER_PORT) + 1);
	m_SSLServer->StartAcceptor();

	StartUDP();
	//Acceptor();
	

	while (!m_Quit)
	{		
		//Acceptor();
		Sleep(200);
	};
}

bool Server::OnUDPNewRemoteAddress(DWORD dwBytesTransfered, UDPTransport* udpTransport)
{
	if (dwBytesTransfered >= sizeof(uint32_t))
	{
		assert(dwBytesTransfered == sizeof(uint32_t));

		uint32_t* pbuffer = (uint32_t*)udpTransport->GetRecvBuffer();
		uint32_t numOfInts = *pbuffer;

		if (numOfInts)
		{
			return OnRecived(udpTransport);			
		}
		else
		{

			udpTransport->Send((char*)&numOfInts, sizeof(uint32_t));
			return false;
		}
	}
	else
	{
		throw std::logic_error("OnUDPNewRemoteAddress");
	}
	
}

bool Server::OnRecived(Transport* transport)
{	
	if (transport->m_CurrRecived >= sizeof(uint32_t))
	{
		assert(transport->m_CurrRecived == sizeof(uint32_t));
		
		uint32_t numOfInts = *(uint32_t*)transport->GetRecvBuffer();
				
		
		HLTransportData* hlTransportData = GetHLTransportData(transport);
		
		auto& currentNumOffset = hlTransportData->m_CurrentNumOffset;

		if (currentNumOffset >= g_NETDATA_SIZE)
			currentNumOffset = 0;

		auto netdata = g_NETDATA + currentNumOffset;
		assert(numOfInts <= g_NETDATA_SIZE - currentNumOffset);

		currentNumOffset += numOfInts;
		transport->m_CurrRecived -= sizeof(uint32_t);

		transport->Send((char*)netdata, numOfInts * sizeof(uint32_t));		
		
	//	++pbuffer;
	}
	
	return true;
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






