#pragma once

#include "base.h"

extern const char * server_cert_key_pem;

unsigned __stdcall ServerMain(void*);





class Server : public HL_IOCP
{
public:	
	volatile bool m_Quit;
	
	Server();
		
	void ThreadMain();

	virtual bool OnUDPNewRemoteAddress(DWORD dwBytesTransfered, UDPTransport* udpTransport) override;
	virtual bool OnRecived(Transport* transport) override;

	
};