#pragma once
#include "base.h"

unsigned __stdcall ClientMain(void*);




class Client : public HL_IOCP
{
public:
	volatile bool m_Quit;

	void ThreadMain();

	HANDLE m_hSignal;
	CRITICAL_SECTION m_ParseQueueCS;
	queue<TCPTransport*, deque<TCPTransport*> > m_ParseQueue;
	
	
	SOCKET NewUDPSocket();
	
	std::string m_ServerAddress;
	std::string	m_ServerPort;

	void SetupAdditionalConnections();
	virtual bool OnUDPNewRemoteAddress(DWORD dwBytesTransfered, UDPTransport* udpTransport) override;
	virtual bool OnTransportConnected(TCPTransport* transport) override;
	virtual bool OnRecived(Transport* transport) override;

	Client();
	~Client();
private:
	void Validate(Transport* transport);
	void FastValidate(Transport* transport);


	void AddTransportForProcess(TCPTransport* transport);
	void ProcessTransport(Transport* transport);
};



