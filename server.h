#pragma once

#include "base.h"


unsigned __stdcall ServerMain(void*);


class Server : public IOCPBase
{
public:	
	volatile bool m_Quit;

	SOCKET m_ListenSocket;	
	
	void ThreadMain();

	void Acceptor();

	LPFN_ACCEPTEX m_lpfnAcceptEx;
	Transport * m_AcceptedTransport;
	void StartTCPAcceptor();
	void ResumeTCPAccept();

	SOCKET m_UDPSocket;
	void StartUDPSocket();
		
	virtual void RemoveTransport(Transport* client) override;
	
	void CreateListenSocket();
	void DeleteListenSocket();

	virtual bool OnRecived(Transport* transport, DWORD dwBytesTransfered) override;

	Server();
	~Server();	
	
	
};