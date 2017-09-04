#include "stdafx.h"
#include "Transport.h"
#include "base.h"
#include "Exceptions.h"
#include "cert.h"




Socketable::Socketable(IOCPBase* iocpBase, SOCKET clientSocket, bool isUdp /*= false*/) : m_IOCPBaseParent(iocpBase),
															m_Socket(clientSocket),
															m_IsUDPTransport(isUdp)
{

}

Socketable::~Socketable()
{

}


Transport::Transport(IOCPBase* iocpBase, SOCKET clientSocket, bool isUdp /*= false*/) : Socketable(iocpBase, clientSocket, isUdp),
																			m_TotalRecived(0),
																			m_CurrRecived(0),
																			m_CurrProcessed(0),
																			m_ErrorCounter(0)
{
	if (m_Socket != INVALID_SOCKET)
		SetNonBlockingMode();

	//ZeroMemory(&m_SendOverlapped, sizeof(m_SendOverlapped));
	//m_SendOverlapped.hEvent = WSACreateEvent();
	//if (m_SendOverlapped.hEvent == WSA_INVALID_EVENT)
	//	throw EXCEPTION(SystemException());	
}




Transport::~Transport()
{
	//if (m_SendOverlapped.hEvent != 0)
	//{
	//	WSACloseEvent(m_SendOverlapped.hEvent);
	//}

}



void Transport::SetNonBlockingMode()
{
	u_long value = 1;
	ioctlsocket(m_Socket, FIONBIO, &value);
}

TCPTransport::TCPTransport(IOCPBase* iocpBase, SOCKET clientSocket) : Transport(iocpBase, clientSocket, false)
{
	m_RecvBuffer.resize(sizeof(g_NETDATA) * 5);

	ZeroMemory(&m_ReciveOverlapped, sizeof(m_ReciveOverlapped));
	m_ReciveOverlapped.hEvent = WSACreateEvent();
	if (m_ReciveOverlapped.hEvent == WSA_INVALID_EVENT)
		throw EXCEPTION(SystemException());
}


TCPTransport::~TCPTransport()
{
	if (m_ReciveOverlapped.hEvent != 0)
	{
		WSACloseEvent(m_ReciveOverlapped.hEvent);
	}

	if (m_Socket != INVALID_SOCKET)
	{
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
	}
}


void TCPTransport::Connect(const string& address, const string& port)
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
	if (getaddrinfo(address.c_str(), port.c_str(), &hints, &resolvedServerAddress))
		throw EXCEPTION(WSAException());



	// Attempt to connect to an address until one succeeds
	for (ptr = resolvedServerAddress; ptr != NULL; ptr = ptr->ai_next)
	{

		// Create a SOCKET for connecting to TCPSERVER		
		//socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

		if (INVALID_SOCKET == socket)
		{
			throw EXCEPTION(WSAException());
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
		throw EXCEPTION(WSAException());

	m_Socket = socket;

	SetNonBlockingMode();
}

TCPServer::TCPServer(IOCPBase* iocpBase, int port) : Socketable(iocpBase, CreateListenSocket(port), false)
{

}

TCPServer::~TCPServer()
{
	if (m_Socket != INVALID_SOCKET)
	{
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
	}
}

void TCPServer::Complition(DWORD dwBytesTransfered, OVERLAPPED* overlapped)
{
	int iResult = setsockopt(m_AcceptedTransport->m_Socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
		(char *)&m_Socket, sizeof(m_Socket));

	TCPTransport* transport = m_AcceptedTransport;

	AcceptNext();

	//struct sockaddr* localAddress = nullptr;
	//struct sockaddr* remoteAddress = nullptr;
	//
	//INT localAddressSize = 0;
	//INT remoteAddressSize = 0;

	//g_lpfnGetAcceptExSockaddrs((PVOID)transport->m_RecvBuffer.data(), dwBytesTransfered, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
	//	&localAddress, &localAddressSize, &remoteAddress, &remoteAddressSize);

	

	m_IOCPBaseParent->AddTransport(transport);
	//dwBytesTransfered -= (sizeof(sockaddr_in) + 16) * 2;

	if (dwBytesTransfered > 0)
	{
		transport->Complition(dwBytesTransfered, nullptr);

		//if (!m_IOCPBaseParent->OnRecived(transport, dwBytesTransfered))
		//{
		//	m_IOCPBaseParent->RemoveTransport(transport);
		//	return;
		//}

	//	transport->StartReceiving();
	}
	else
	{
		transport->StartReceiving();
	}
}

void TCPServer::StartAcceptor()
{
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)m_Socket, m_IOCPBaseParent->m_hIOCompletionPort, (ULONG_PTR)static_cast<Socketable*>(this), 0);


	if (NULL == hTemp)
	{
		WriteToConsole("AssociateWithIOCP GetLastError == %i", GetLastError());
		throw EXCEPTION(SystemException());
	}

	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	DWORD dwBytes = 0;

	int iResult = WSAIoctl(m_Socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx, sizeof(GuidAcceptEx),
		&g_lpfnAcceptEx, sizeof(g_lpfnAcceptEx),
		&dwBytes, NULL, NULL);

	if (iResult == SOCKET_ERROR)
	{
		throw EXCEPTION(SystemException());
	}

	GUID GuidGetAcceptexSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;

	iResult = WSAIoctl(m_Socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx, sizeof(GuidAcceptEx),
		&g_lpfnGetAcceptExSockaddrs, sizeof(g_lpfnGetAcceptExSockaddrs),
		&dwBytes, NULL, NULL);

	if (iResult == SOCKET_ERROR)
	{
		throw EXCEPTION(SystemException());
	}

	//assert(dwBytes == 0);


	AcceptNext();
}

SOCKET TCPServer::CreateListenSocket(int port)
{
	//Overlapped I/O follows the model established in Windows and can be performed only on 
	//sockets created through the WSASocket function 
	SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == socket)
	{
		throw EXCEPTION(SystemException());
	}

	struct sockaddr_in serverAddress;
	//Cleanup and Init with 0 the ServerAddress
	ZeroMemory((char *)&serverAddress, sizeof(serverAddress));

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY; //WinSock will supply address
	serverAddress.sin_port = htons(port);    //comes from commandline
											 //Assign local address and port number
	if (SOCKET_ERROR == bind(socket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)))
		throw EXCEPTION(SystemException());

	WriteToConsole("\nbind() successful.");


	//Make the socket a listening socket
	if (SOCKET_ERROR == listen(socket, SOMAXCONN))
		throw EXCEPTION(SystemException());

	WriteToConsole("\nlisten() successful.");

	return socket;
}



void TCPServer::AcceptNext()
{
	SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (socket == INVALID_SOCKET)
	{
		throw EXCEPTION(SystemException());
	}

	auto tcpTransport = m_IOCPBaseParent->NewTCPTransport(socket);

	m_IOCPBaseParent->AssociateWithIOCP(tcpTransport);
	//AddTransport(transport);


	BOOL result = g_lpfnAcceptEx(m_Socket, socket, (PVOID)tcpTransport->m_RecvBuffer.data(), tcpTransport->m_RecvBuffer.size() - ((sizeof(sockaddr_in) + 16) * 2),
		sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
		NULL, &tcpTransport->m_ReciveOverlapped);


	if (result == FALSE && WSAGetLastError() != ERROR_IO_PENDING)
	{
		WriteToConsole("AcceptEx Error! WSAGetLastError %i", WSAGetLastError());
		throw EXCEPTION(SystemException());
	}

	m_AcceptedTransport = tcpTransport;
}

void TCPTransport::Send(char* data, ULONG size)
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

	int result = send(m_Socket, data, size, 0);

	if (result == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		if (error != WSAEWOULDBLOCK)
			throw EXCEPTION(WSAException(m_Socket, error));
	}
}

void TCPTransport::StartReceiving()
{
	DWORD dwBytes = 0;
	DWORD flag = 0/*MSG_PARTIAL*/;

	WSABUF buffer;
	buffer.buf = (CHAR*)m_RecvBuffer.data() + m_CurrRecived;
	buffer.len = m_RecvBuffer.size() - m_CurrRecived;

	assert(buffer.len > 0);

	auto result = WSARecv(m_Socket, &buffer, 1, NULL, &flag, &m_ReciveOverlapped, NULL);

	if (result == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		if (error != WSA_IO_PENDING)
			throw EXCEPTION(WSAException(m_Socket, error));
	}
}


BYTE* TCPTransport::GetRecvBuffer()
{
	return m_RecvBuffer.data();
}

void TCPTransport::Complition(DWORD dwBytesTransfered, OVERLAPPED*)
{
	if (dwBytesTransfered > 0)
	{
		m_CurrRecived += dwBytesTransfered;
		m_TotalRecived += dwBytesTransfered;

		if (!m_IOCPBaseParent->OnRecived(this))
		{
			m_IOCPBaseParent->RemoveTransport(this);
			return;
		}

		StartReceiving();

		//if (!transport->Recive())
		//{
		//	WriteToConsole("%s: transport->Recive error", m_Prefix.c_str());
		//	RemoveTransport(transport);
		//	continue;
		//}
	}
	else
	{
		WriteToConsole("%s: transport shutdown", m_IOCPBaseParent->m_Prefix.c_str());
		m_IOCPBaseParent->RemoveTransport(this);
	}
}

void SSLTransport::Send(char* data, ULONG size)
{	
	int	ret = SSL_write(m_Ssl, data, size);
	int	ssl_error = SSL_get_error(m_Ssl, ret);

	if (IsSSLError(ssl_error))
		throw EXCEPTION(SSLException(m_Ssl, ret));

	SSLProcessingSend();	
}

void SSLTransport::StartReceiving()
{
	DWORD dwBytes = 0;
	DWORD flag = 0/*MSG_PARTIAL*/;

	WSABUF buffer;
	buffer.buf = (CHAR*)m_RecvBuffer.data();
	buffer.len = m_RecvBuffer.size();

	assert(buffer.len > 0);

	auto result = WSARecv(m_Socket, &buffer, 1, NULL, &flag, &m_ReciveOverlapped, NULL);

	if (result == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		if (error != WSA_IO_PENDING)
			throw EXCEPTION(WSAException(m_Socket, error));
	}
}

void SSLTransport::Complition(DWORD dwBytesTransfered, OVERLAPPED*)
{
	if (dwBytesTransfered > 0)
	{
		m_BytesSizeRecieved += dwBytesTransfered;
		SSLProcessingRecv();

		StartReceiving();

		//if (!transport->Recive())
		//{
		//	WriteToConsole("%s: transport->Recive error", m_Prefix.c_str());
		//	RemoveTransport(transport);
		//	continue;
		//}
	}
	else
	{
		WriteToConsole("%s: transport shutdown", m_IOCPBaseParent->m_Prefix.c_str());
		m_IOCPBaseParent->RemoveTransport(this);
	}	
}

void SSLTransport::Connect(const string& address, const string& port)
{
	TCPTransport::Connect(address, port);
	CreateClientSSLContext();
	SSL_set_connect_state(m_Ssl);
	SSLProcessingConnect();
}


void SSLTransport::Accept()
{
	CreateServerSSLContext();
	SSL_set_accept_state(m_Ssl);
	SSLProcessingAccept();
}

BYTE* SSLTransport::GetRecvBuffer()
{
	return  m_DecryptedRecvData.data();
}



bool SSLTransport::IsSSLError(int ssl_error)
{
	switch (ssl_error)
	{
		case SSL_ERROR_NONE:
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
				return false;

		default: return true;
	}
	
}

void SSLTransport::SSLProcessingConnect()
{
	int ret;
	int ssl_error;

	{
		DWORD dwBytesSizeRecieved = 0;

		do
		{
			ret = SSL_read(m_Ssl, m_DecryptedRecvData.data(), m_DecryptedRecvData.size());
			ssl_error = SSL_get_error(m_Ssl, ret);

			if (IsSSLError(ssl_error))
				throw EXCEPTION(SSLException(m_Ssl, ret));

			if (ret > 0)
				dwBytesSizeRecieved += ret;

		} while (ret > 0);

		
		if (SSL_is_init_finished(m_Ssl))
		{
			m_Handshaked = true;
			m_IOCPBaseParent->OnTransportConnected(this);
		}		
	}


	SSLProcessingSend();	
}

void SSLTransport::SSLProcessingAccept()
{
	int ret;
	int ssl_error;

	{
		DWORD dwBytesSizeRecieved = 0;

		do
		{
			ret = SSL_read(m_Ssl, m_DecryptedRecvData.data(), m_DecryptedRecvData.size());
			ssl_error = SSL_get_error(m_Ssl, ret);

			if (IsSSLError(ssl_error))
				throw EXCEPTION(SSLException(m_Ssl, ret));

			if (ret > 0)
				dwBytesSizeRecieved += ret;

		} while (ret > 0);

		
		if (SSL_is_init_finished(m_Ssl))
		{
			m_Handshaked = true;
			m_IOCPBaseParent->OnTransportConnected(this);
		}
	}
	
	SSLProcessingSend();

}
	


void SSLTransport::SSLProcessingSend()
{
	int ret;
	int ssl_error;

	while (BIO_pending(m_Bio[SEND]))
	{
		ret = BIO_read(m_Bio[SEND], m_EncryptedSendData.data(), m_EncryptedSendData.size());

		if (ret > 0)
		{
			TCPTransport::Send(reinterpret_cast<char*>(m_EncryptedSendData.data()), ret);
		}
		else
		{
			ssl_error = SSL_get_error(m_Ssl, ret);

			if (IsSSLError(ssl_error))
				throw EXCEPTION(SSLException(m_Ssl, ret));
		}
	}
}

void SSLTransport::SSLProcessingRecv()
{
	int ret;
	int ssl_error;

	if (m_BytesSizeRecieved > 0)
	{
		ret = BIO_write(m_Bio[RECV], m_RecvBuffer.data(), m_BytesSizeRecieved);
		
		if (ret > 0)
		{
			DWORD dwRet = ret;
			if (dwRet > m_BytesSizeRecieved)
				throw EXCEPTION(SSLException(m_Ssl, ret));

			m_BytesSizeRecieved -= dwRet;
		}
		else
		{
			ssl_error = SSL_get_error(m_Ssl, ret);
			if (IsSSLError(ssl_error))
				throw EXCEPTION(SSLException(m_Ssl, ret));
		}

		/*if ( !BIO_should_retry( bio[RECV]) )
		{
		if (!IsSSLError(ssl_error))
		throw EXCEPTION(SSLException(ssl, ret));
		}*/
	}
	
	{		
		do
		{
			assert(m_DecryptedRecvData.size() - m_CurrRecived > 0);
			ret = SSL_read(m_Ssl, m_DecryptedRecvData.data() + m_CurrRecived, m_DecryptedRecvData.size() - m_CurrRecived);			
						
			if (ret > 0)
			{			
				m_CurrRecived += ret;
				m_TotalRecived += ret;

				if (m_Handshaked)
				{
					m_IOCPBaseParent->OnRecived(this);
				}				
			}
			else
			{
				ssl_error = SSL_get_error(m_Ssl, ret);

				if (IsSSLError(ssl_error))
					throw EXCEPTION(SSLException(m_Ssl, ret));
			}
		}
		while (ret > 0);

		if (!m_Handshaked)
		{			
			if (SSL_is_init_finished(m_Ssl))
			{
				m_Handshaked = true;
				m_IOCPBaseParent->OnTransportConnected(this);
			}
		}
	}

	SSLProcessingSend();
}


void SSLTransport::CreateServerSSLContext()
{
	m_SslMethod = SSLv23_server_method();
	m_SslCtx = SSL_CTX_new(m_SslMethod);
	SSL_CTX_set_verify(m_SslCtx, SSL_VERIFY_NONE, nullptr);


	SetSSLCertificate();

	m_Ssl = SSL_new(m_SslCtx);
	//SSL_set_verify(ssl, SSL_VERIFY_NONE, nullptr);

	m_Bio[SEND] = BIO_new(BIO_s_mem());
	m_Bio[RECV] = BIO_new(BIO_s_mem());
	SSL_set_bio(m_Ssl, m_Bio[RECV], m_Bio[SEND]);
}

void SSLTransport::CreateClientSSLContext()
{
	m_SslMethod = SSLv23_method();
	m_SslCtx = SSL_CTX_new(m_SslMethod);
	SSL_CTX_set_verify(m_SslCtx, SSL_VERIFY_NONE, nullptr);

	m_Ssl = SSL_new(m_SslCtx);
//	SSL_set_verify(ssl, SSL_VERIFY_NONE, nullptr);

	m_Bio[SEND] = BIO_new(BIO_s_mem());
	m_Bio[RECV] = BIO_new(BIO_s_mem());
	SSL_set_bio(m_Ssl, m_Bio[RECV], m_Bio[SEND]);
}

//void SSLTransport::SSLRecv()
//{
//	
//}

SSLTransport::SSLTransport(IOCPBase* iocpBase, SOCKET clientSocket) : TCPTransport(iocpBase, clientSocket), m_Handshaked(false), m_BytesSizeRecieved(0)
{
/*	ssl_buffer.resize(sizeof(g_NETDATA) * 5);	*/
	m_EncryptedSendData.resize(sizeof(g_NETDATA) * 5);
	m_DecryptedRecvData.resize(sizeof(g_NETDATA) * 5);	

}

SSLTransport::~SSLTransport()
{

}

void SSLTransport::SetSSLCertificate()
{
	int length = strlen(server_cert_key_pem);
	BIO *bio_cert = BIO_new_mem_buf((void*)server_cert_key_pem, length);
	X509 *cert = PEM_read_bio_X509(bio_cert, nullptr, nullptr, nullptr);
	EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio_cert, 0, 0, 0);


	int ret = SSL_CTX_use_certificate(m_SslCtx, cert);
		
	if (ret != 1)
		throw EXCEPTION( SSLException(m_Ssl,ret) );

	ret = SSL_CTX_use_PrivateKey(m_SslCtx, pkey);

	if (ret != 1)
		throw EXCEPTION(SSLException(m_Ssl, ret));

	X509_free(cert);
	EVP_PKEY_free(pkey);
	BIO_free(bio_cert);
}

void UDPTransport::Send(char* data, ULONG size)
{
	int result = sendto(m_Socket, data, size, 0, (struct sockaddr*)&m_SendAddress, m_SendAddressLen);

	if (result == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		if (error != WSAEWOULDBLOCK)
			throw EXCEPTION(WSAException(m_Socket, error));
	}
}

BYTE* UDPTransport::GetRecvBuffer()
{
	return m_MainTransport->m_RecvBuffer.data();
}

void UDPTransport::Complition(DWORD dwBytesTransfered, OVERLAPPED* overlapped)
{
	assert(0);
}

UDPTransport::UDPTransport(IOCPBase* iocpBase, UDPDomain* udpDomain) : Transport(iocpBase, udpDomain->m_Socket, true),
m_MainTransport(udpDomain),	
	m_SendAddress(udpDomain->m_RecvAddress),
	m_SendAddressLen(sizeof(m_SendAddress))
{
//	m_IsUDPTransport = true;
}

const struct sockaddr_in UDPDomain::scm_BroadcastAddress = { AF_INET, htons((USHORT)atoi(SERVER_PORT)), {255,255,255,255} ,{ 0, } };

void UDPDomain::SendBroadcast(char* data, ULONG size)
{

	//int sizeof_broadcastOptValue = sizeof(broadcastOptValue);
	//
	//int result = getsockopt(m_ClientSocket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcastOptValue, &sizeof_broadcastOptValue);

	//if (result == 0)
	//{
	//	return false;
	//}
	BOOL broadcastOptValue = TRUE;
	if (setsockopt(m_Socket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcastOptValue, sizeof(broadcastOptValue)) != 0)
	{
		throw EXCEPTION( WSAException(m_Socket) );
	}

	int result = sendto(m_Socket, data, size, 0, (struct sockaddr*)&scm_BroadcastAddress, sizeof(scm_BroadcastAddress));

	int error = WSAGetLastError();

	if (result == SOCKET_ERROR && error != WSAEWOULDBLOCK)
	{		
		throw EXCEPTION(WSAException(m_Socket, error));
	}

	broadcastOptValue = FALSE;
	if (setsockopt(m_Socket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcastOptValue, sizeof(broadcastOptValue)) != 0)
	{
		throw EXCEPTION(WSAException(m_Socket));
	}
}


void UDPDomain::Bind(const char* port)
{
	struct sockaddr_in serverAddress;

	ZeroMemory((char *)&serverAddress, sizeof(serverAddress));

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	serverAddress.sin_port = htons(atoi(port));

	if (SOCKET_ERROR == bind(m_Socket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)))
	{		
		throw EXCEPTION(WSAException());
	}
}

UDPDomain::UDPAssociatedAddress UDPDomain::MakeAssociatedAddress(const struct sockaddr_in& address)
{
	return uint64_t(address.sin_addr.S_un.S_addr) + ( uint64_t(address.sin_port) << (sizeof(address.sin_addr.S_un.S_addr) * 8));
}

void UDPDomain::OnRecv(DWORD dwBytesTransfered)
{
	
}

void UDPDomain::Remove(UDPTransport* transport)
{
	auto addr = MakeAssociatedAddress(transport->m_SendAddress);	
	m_sockaddr2UDPTransports.erase(addr);
}

void UDPDomain::StartReceiving()
{
	DWORD dwBytes = 0;
	DWORD flag = 0;

	WSABUF buffer;
	//buffer.buf = (CHAR*)m_RecvBuffer.data() + m_CurrRecived;
	//buffer.len = m_RecvBuffer.size() - m_CurrRecived;

	buffer.buf = (CHAR*)m_RecvBuffer.data();
	buffer.len = m_RecvBuffer.size();

	assert(buffer.len > 0);

	auto result = WSARecvFrom(m_Socket, &buffer, 1, NULL, &flag, (struct sockaddr *) &m_RecvAddress, &m_RecvAddressLen, &m_ReciveOverlapped, NULL);

	if (result == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		if (error != WSA_IO_PENDING)
			throw EXCEPTION(WSAException(m_Socket, error));
	}
}

void UDPDomain::Complition(DWORD dwBytesTransfered, OVERLAPPED*)
{
	auto address = MakeAssociatedAddress(m_RecvAddress);

	auto it = m_sockaddr2UDPTransports.find(address);

	if (it != m_sockaddr2UDPTransports.end())
	{
		UDPTransport* udpTransport = it->second;
		udpTransport->m_CurrRecived += dwBytesTransfered;
		udpTransport->m_TotalRecived += dwBytesTransfered;

		if (!m_IOCPBaseParent->OnRecived(udpTransport))
		{
			m_IOCPBaseParent->RemoveTransport(udpTransport);
			//return false;
		}
	}
	else
	{
		auto newUDPTransport = m_IOCPBaseParent->NewUDPTransport(this);
		newUDPTransport->m_CurrRecived += dwBytesTransfered;
		newUDPTransport->m_TotalRecived += dwBytesTransfered;
		if (m_IOCPBaseParent->OnUDPNewRemoteAddress(dwBytesTransfered, newUDPTransport))
		{
			auto newPair = make_pair(address, newUDPTransport);
			m_sockaddr2UDPTransports.insert(newPair);
			m_IOCPBaseParent->AddTransport(newUDPTransport);
		}
		else
		{
			delete newUDPTransport;			
		}		
	}

	StartReceiving();
}

UDPDomain::UDPDomain(IOCPBase* iocpBase) : Socketable(iocpBase, INVALID_SOCKET, true)
{
	m_Socket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == m_Socket)
	{
		throw EXCEPTION(WSAException());
	}

	ZeroMemory(&m_ReciveOverlapped, sizeof(m_ReciveOverlapped));
	m_ReciveOverlapped.hEvent = WSACreateEvent();
	if (m_ReciveOverlapped.hEvent == WSA_INVALID_EVENT)
		throw EXCEPTION(WSAException());

	ZeroMemory(&m_RecvAddress, sizeof(m_RecvAddress));
	m_RecvAddressLen = sizeof(m_RecvAddress);

	m_RecvBuffer.resize(sizeof(g_NETDATA) * 5);

	m_IOCPBaseParent->AssociateWithIOCP(this);
}

void SSLServer::AcceptNext()
{
		SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

		if (socket == INVALID_SOCKET)
		{
			throw EXCEPTION(SystemException());
		}

		auto transport = m_IOCPBaseParent->NewSSLTransport(socket);		


		m_IOCPBaseParent->AssociateWithIOCP(transport);
		//AddTransport(transport);


		BOOL result = g_lpfnAcceptEx(m_Socket, socket, (PVOID)transport->m_RecvBuffer.data(), transport->m_RecvBuffer.size() - ((sizeof(sockaddr_in) + 16) * 2),
			sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
			NULL, &transport->m_ReciveOverlapped);


		if (result == FALSE && WSAGetLastError() != ERROR_IO_PENDING)
		{
			WriteToConsole("AcceptEx Error! WSAGetLastError %i", WSAGetLastError());
			throw EXCEPTION(SystemException());
		}

		m_AcceptedTransport = transport;
	
}

void SSLServer::Complition(DWORD dwBytesTransfered, OVERLAPPED* overlapped)
{
	static_cast<SSLTransport*>(m_AcceptedTransport)->Accept();
	TCPServer::Complition(dwBytesTransfered, overlapped);
}
