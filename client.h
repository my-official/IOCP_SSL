#pragma once
#include "base.h"

unsigned __stdcall ClientMain(void*);




class Client : public IOCPBase
{
public:
	volatile bool m_Quit;

	void ThreadMain();

	HANDLE m_hSignal;
	CRITICAL_SECTION m_ParseQueueCS;
	queue<Transport*, deque<Transport*> > m_ParseQueue;
	

	SOCKET NewTCPConnection();
	SOCKET NewUDPConnection();

	virtual bool OnRecived(Transport* transport, DWORD dwBytesTransfered) override;

	Client();
	~Client();
private:
	void Validate(Transport* transport);
	void FastValidate(Transport* transport);


	void AddTransportForProcess(Transport* transport);
	void ProcessTransport(Transport* transport);
};



