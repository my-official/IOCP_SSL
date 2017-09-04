#pragma once

#include "Threads.h"
#include "Exceptions.h"

const char SERVER_PORT[] = "55555";

extern CRITICAL_SECTION g_ConsoleCS;


void WriteToConsole(char *szFormat, ...);


extern LPFN_ACCEPTEX g_lpfnAcceptEx;
extern LPFN_GETACCEPTEXSOCKADDRS g_lpfnGetAcceptExSockaddrs;

const uint32_t g_NETDATA_SIZE = 30000;
extern uint32_t g_NETDATA[g_NETDATA_SIZE];

class Socketable;
class Transport;
class TCPTransport;
class SSLTransport;
class UDPTransport;

class UDPDomain;
class TCPServer;
class SSLServer;




class IOCPBase
{	
public:
	IOCPBase();
	virtual ~IOCPBase();

	HANDLE m_hIOCompletionPort;
			
	virtual void Recv();

	CRITICAL_SECTION		m_Socket2TransportsCS;
	map<SOCKET, Transport* > m_Socket2Transports;		


	void AssociateWithIOCP(Socketable* socketable);	
	void AddTransport(Transport* transport);

	virtual void RemoveTransport(Transport* client);
		
	void CreateIOCP();
	void CreateRecvThreadPool();	
	
	void DestroyRecvThreadPool();
	void DestroyIOCP();

	string m_Prefix;
	void PrintTotalRecived();


	/////////////TCP
	TCPServer* m_TCPServer;	
	
	////////////SSL
	SSLServer* m_SSLServer;

	/////////////UDP	
	UDPDomain* m_UDPDomain;
	void StartUDP();
	
	virtual TCPTransport* NewTCPTransport(SOCKET socket = INVALID_SOCKET);
	virtual UDPTransport* NewUDPTransport(UDPDomain* udpDomain);
	virtual SSLTransport* NewSSLTransport(SOCKET socket = INVALID_SOCKET);
public:
	virtual bool OnTransportConnected(TCPTransport* transport) { return true; }
	virtual bool OnUDPNewRemoteAddress(DWORD dwBytesTransfered, UDPTransport* udpTransport) = 0;
	virtual bool OnRecived(Transport* transport) = 0;

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


class HLTransportData
{
public:
	uint32_t m_CurrentNumOffset = 0;
};

template <class TransportT>
class HLTransport : public TransportT, public HLTransportData
{
public:
	using TransportT::TransportT;
};




class HL_IOCP : public IOCPBase
{
public:
	HLTransportData* GetHLTransportData(Transport* transport);

	virtual TCPTransport* NewTCPTransport(SOCKET socket = INVALID_SOCKET) override;
	virtual UDPTransport* NewUDPTransport(UDPDomain* udpDomain) override;
	virtual SSLTransport* NewSSLTransport(SOCKET socket = INVALID_SOCKET) override;
};