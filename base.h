#pragma once

#include "Threads.h"
#include "Exceptions.h"

const char SERVER_PORT[] = "5500";

extern CRITICAL_SECTION g_ConsoleCS;


void WriteToConsole(char *szFormat, ...);



const uint32_t g_NETDATA_SIZE = 15000;
extern uint32_t g_NETDATA[g_NETDATA_SIZE];


class Transport;

class IOCPBase
{	
public:
	IOCPBase();
	virtual ~IOCPBase();
protected:
	HANDLE m_hIOCompletionPort;
			
	void Recv();

	CRITICAL_SECTION		m_Socket2ClientsCS;
	map<SOCKET, Transport* > m_Socket2Clients;

	void AssociateWithIOCP(Transport* transport);
	virtual void RemoveTransport(Transport* client);
		
	void CreateIOCP();
	void CreateRecvThreadPool();	
	
	void DestroyRecvThreadPool();
	void DestroyIOCP();

	string m_Prefix;
	void PrintTotalRecived();


	virtual Transport* NewTransport(SOCKET socket);
	virtual UDPTransport* NewUDPTransport(SOCKET socket);

public:
	virtual bool OnRecived(Transport* transport, DWORD dwBytesTransfered) = 0;

private:
	static DWORD sm_NumRecvThreads;

	class RecvThread : public Thread
	{
	public:
		RecvThread(IOCPBase* parent);
//	protected:
		IOCPBase* m_Parent;
		virtual void ThreadMain() override;
		virtual void SendQuit() override;
	};

	vector<RecvThread*> m_RecvThreads;


	class TimerThread : public Thread
	{
	public:
		static const int DelayMilliseconds;
		TimerThread(IOCPBase* parent);
		//	protected:
		IOCPBase* m_Parent;
		virtual void ThreadMain() override;
		virtual void SendQuit() override;
	private:
		volatile bool m_Quit;
	};

	TimerThread m_TimerThread;
};



class Transport
{
public:	
	IOCPBase* m_IOCPBaseParent;

	SOCKET m_ClientSocket;
		
		

	WSAOVERLAPPED m_SendOverlapped;
	virtual bool Send(char* data, ULONG size);


	vector<BYTE> m_RecvBuffer;

	DWORD m_TotalRecived;
	DWORD m_CurrRecived;
	DWORD m_CurrProcessed;

	uint32_t m_ErrorCounter; 


	OVERLAPPED m_ReciveOverlapped;
	virtual bool Recive();




	Transport(IOCPBase* iocpBase, SOCKET clientSocket);
	~Transport();
};


class UDPTransport : public Transport
{
public:	
	struct sockaddr		m_Address;
	INT					m_AddressLen;

	virtual bool Send(char* data, ULONG size);
	virtual bool Recive();

	UDPTransport(IOCPBase* iocpBase, SOCKET clientSocket);	
};