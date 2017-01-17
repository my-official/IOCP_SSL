#pragma once

#include "base.h"


unsigned __stdcall ServerMain(void*);


class Server : public IOCPBase
{
public:
	typedef void  (Server::*RecvFunc)(SOCKET*);
	void Recv(SOCKET* socket);
private:
	SOCKET m_ListenSocket = INVALID_SOCKET;	

	class Transport
	{
	public:		
		int m_Id;

		Server* m_Server;
		SOCKET m_ClientSocket;
		DWORD m_TotalRecived;

		vector<BYTE> m_RecvBuffer;
		
		OVERLAPPED m_SendOverlapped;
		bool Send(DWORD numOfInts);
		
		OVERLAPPED m_ReciveOverlapped;
		bool Recive();		

		bool OnRecived(DWORD dwBytesTransfered);		

		Transport(Server* server, SOCKET clientSocket);
		~Transport();
	};

	map<SOCKET, Transport* > m_Socket2Clients;	

	friend unsigned __stdcall ServerMain(void*);
	void ThreadMain();	
	
	virtual void RecvThread() override;

	void Acceptor();	
	void AssociateWithIOCP(Server::Transport * client);
	
	void CreateListenSocket();	;	
	void RemoveClient(Transport* client);
};