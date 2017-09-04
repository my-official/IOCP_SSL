#include "client.h"
#include "Transport.h"

const uint32_t g_NumUInt32ForRecv = 15000;
const uint32_t g_BroadcastData = 0;

void Client::ThreadMain()
{		
	m_UDPDomain = new UDPDomain(this);
	
	m_UDPDomain->SendBroadcast((char*)&g_BroadcastData, sizeof(g_BroadcastData));
	m_UDPDomain->StartReceiving();
	
	
	while (!m_Quit)
	{
		Sleep(200);
	}
}


SOCKET Client::NewUDPSocket()
{
	SOCKET socket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == socket)
	{
		throw EXCEPTION(WSAException());
	}

	return socket;
}

void Client::SetupAdditionalConnections()
{
	for (int c = 0, size = 25; c < size; c++)
	{		
		TCPTransport* transport = NewTCPTransport();		

		transport->Connect(m_ServerAddress, m_ServerPort);

		AssociateWithIOCP(transport);
		AddTransport(transport);

		
		transport->StartReceiving();
		transport->Send((char*)&g_NumUInt32ForRecv, sizeof(g_NumUInt32ForRecv));
		
		static_assert(g_NumUInt32ForRecv <= g_NETDATA_SIZE, "g_NumUInt32ForRecv <= g_NETDATA_SIZE");		
	}

	for (int c = 0, size = 10; c < size; c++)
	{
		SSLTransport* transport = NewSSLTransport();

		transport->Connect(m_ServerAddress,  to_string( (stoi(m_ServerPort) + 1) ) );
		//transport->Connect("localhost", "65000");
		AssociateWithIOCP(transport);
		AddTransport(transport);		
		transport->StartReceiving();

		static_assert(g_NumUInt32ForRecv <= g_NETDATA_SIZE, "g_NumUInt32ForRecv <= g_NETDATA_SIZE");
	}

	//for (int c = 0, size = 1; c < size; c++)
	//{
	//	UDPTransport* transport = NewUDPTransport(m_UDPDomain);
	//	AddTransport(transport);

	//	transport->Send((char*)&g_NumUInt32ForRecv, sizeof(g_NumUInt32ForRecv));
	//}

	for (int c = 0, size = 1; c < size; c++)
	{
		UDPDomain* domain = new UDPDomain(this);
		/*AddTransport(domain);

		assert(numUInt32ForRecv <= g_NETDATA_SIZE);*/

	//	domain->Send((char*)&g_NumUInt32ForRecv, sizeof(g_NumUInt32ForRecv));
	}
}

bool Client::OnUDPNewRemoteAddress(DWORD dwBytesTransfered, UDPTransport* udpTransport)
{
	if (m_ServerAddress.empty())
	{
		char* addr = inet_ntoa(m_UDPDomain->m_RecvAddress.sin_addr);
		if (!addr)
			throw EXCEPTION(WSAException(udpTransport->m_Socket));
		m_ServerAddress = addr;
		m_ServerPort = to_string(htons(m_UDPDomain->m_RecvAddress.sin_port));

		SetupAdditionalConnections();
	}

	udpTransport->m_CurrRecived = 0;
	udpTransport->Send((char*)&g_NumUInt32ForRecv, sizeof(g_NumUInt32ForRecv));
	return true;

	//return false;
}

bool Client::OnTransportConnected(TCPTransport* transport)
{	
	transport->Send((char*)&g_NumUInt32ForRecv, sizeof(g_NumUInt32ForRecv));
	return true;
}

bool Client::OnRecived(Transport* transport)
{	
	if (transport->m_CurrRecived >= g_NumUInt32ForRecv * sizeof(uint32_t))
	{		
		//assert(transport->m_CurrRecived == g_NumUInt32ForRecv * sizeof(uint32_t));
		
		ProcessTransport(transport);

		//AddTransportForProcess(transport);

		transport->m_CurrRecived = 0;
	}

	//if (!transport->Recive())
	//{
	//	WriteToConsole("%s: transport->Recive error", m_Prefix.c_str());
	//	return false;
	//}

	return true;
}

Client::Client() : m_Quit(false), m_hSignal(NULL)
{
	m_Prefix = "Client";
	

	m_hSignal = CreateEvent(NULL, FALSE, FALSE, "ClientSiganl");

	if (m_hSignal == NULL)
	{
		throw EXCEPTION(SystemException());
	}

	ZeroMemory(&m_ParseQueueCS, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(&m_ParseQueueCS);	
}

Client::~Client()
{		
	CloseHandle(m_hSignal);

	DeleteCriticalSection(&m_ParseQueueCS);
	ZeroMemory(&m_ParseQueueCS, sizeof(CRITICAL_SECTION));
}

void Client::Validate(Transport* transport)
{
	uint32_t* buffer = (uint32_t*)transport->GetRecvBuffer();
	uint32_t control_value = buffer[0];
	for (uint32_t c = 0, size = transport->m_CurrRecived / sizeof(uint32_t); c < size; c++)
	{
		transport->m_ErrorCounter += (buffer[c] != control_value++);
	}
}

void Client::FastValidate(Transport* transport)
{
	uint32_t* buffer = (uint32_t*)transport->GetRecvBuffer();

	HLTransportData* hlTransportData = GetHLTransportData(transport);		
	auto& currentNumOffset = hlTransportData->m_CurrentNumOffset;
			
	//if ( transport->m_CurrRecived < (numUInt32ForRecv * sizeof(uint32_t)) )
	//{
	//	transport->m_ErrorCounter += 1000000;

	//}
	//else
	{
		transport->m_ErrorCounter += buffer[0] != currentNumOffset;
		transport->m_ErrorCounter += (buffer[g_NumUInt32ForRecv - 1] != (g_NumUInt32ForRecv + currentNumOffset - 1));

		currentNumOffset += g_NumUInt32ForRecv;
		if (currentNumOffset >= g_NETDATA_SIZE)
		{
			currentNumOffset = 0;
		}
	}
	
	
}

void Client::AddTransportForProcess(TCPTransport* transport)
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
	static_assert(g_NumUInt32ForRecv <= g_NETDATA_SIZE, "g_NumUInt32ForRecv <= g_NETDATA_SIZE");	
	//transport->Send((char*)&numUInt32ForRecv, sizeof(numUInt32ForRecv));
	
	//assert(transport->m_CurrRecived == 0);

	//if (!transport->Recive())
	//{
	//	throw EXCEPTION(SystemException());
	//	WriteToConsole("%s: transport->Recive error", m_Prefix.c_str());
	//	RemoveTransport(transport);
	//}

	transport->Send((char*)&g_NumUInt32ForRecv, sizeof(g_NumUInt32ForRecv));

	//if (!)
	//{
	////	throw EXCEPTION(SystemException());
	//	WriteToConsole("%s: transport->Send error", m_Prefix.c_str());		
	//	RemoveTransport(transport);
	//}
}
