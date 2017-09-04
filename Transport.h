#pragma once

#include "base.h"


class IOCPBase;

class Socketable
{
public:
	IOCPBase* m_IOCPBaseParent;
	SOCKET m_Socket;
	bool m_IsUDPTransport;

	virtual void Complition(DWORD dwBytesTransfered, OVERLAPPED* overlapped) = 0;

	Socketable(IOCPBase* iocpBase, SOCKET clientSocket, bool isUdp = false);
	virtual ~Socketable();
};


class Transport : public Socketable
{
public:
	Transport(IOCPBase* iocpBase, SOCKET clientSocket, bool isUdp = false);
	virtual ~Transport();


	DWORD m_TotalRecived;
	uint32_t m_ErrorCounter;

	DWORD m_CurrRecived;
	DWORD m_CurrProcessed;


	virtual void Send(char* data, ULONG size) = 0;
	//	virtual void Recive() = 0;

	virtual BYTE* GetRecvBuffer() = 0;
protected:
	void SetNonBlockingMode();
};


class TCPTransport : public Transport
{
public:	
	TCPTransport(IOCPBase* iocpBase, SOCKET clientSocket);
	virtual ~TCPTransport();
	
	virtual void Connect(const string& address, const string& port);
	virtual void Send(char* data, ULONG size) override;
		
	vector<BYTE> m_RecvBuffer;
	OVERLAPPED m_ReciveOverlapped;
	virtual void StartReceiving();	
		
	virtual BYTE* GetRecvBuffer() override;

	virtual void Complition(DWORD dwBytesTransfered, OVERLAPPED* overlapped) override;
};

class TCPServer : public Socketable
{
public:
	TCPServer(IOCPBase* iocpBase, int port);
	virtual ~TCPServer();

	virtual void Complition(DWORD dwBytesTransfered, OVERLAPPED* overlapped) override;
	void StartAcceptor();
protected:
	static SOCKET CreateListenSocket(int port);	
	virtual void AcceptNext();
	TCPTransport* m_AcceptedTransport;
};




enum OVERLAPPED_TYPE
{
	RECV = 0,
	SEND = 1,
	CONNECT = 2
};

#define BUFFER_SIZE		2 *64 * 1024

class SSLTransport : public TCPTransport
{
public:
	SSLTransport(IOCPBase* iocpBase, SOCKET clientSocket);
	virtual ~SSLTransport();

	void SetSSLCertificate();

	virtual void Connect(const string& address, const string& port) override;
	virtual void Accept();
	
	virtual void Send(char* data, ULONG size) override;	
	virtual void StartReceiving();

	virtual void Complition(DWORD dwBytesTransfered, OVERLAPPED* overlapped) override;
	virtual BYTE* GetRecvBuffer() override;

private:
	const SSL_METHOD* m_SslMethod;
	SSL_CTX *m_SslCtx;

	bool m_Handshaked;

	vector<BYTE> m_EncryptedSendData;
	vector<BYTE> m_DecryptedRecvData;	
	int m_SendSize;
	
	/*vector<BYTE> ssl_buffer;
	vector<BYTE> m_DecryptedBuffer;*/
	SSL *m_Ssl; // SSL structure used by OpenSSL
	BIO *m_Bio[2]; // memory BIO used by OpenSSL
	
	DWORD m_BytesSizeRecieved;
	

	bool IsSSLError(int ssl_error);

	void SSLProcessingConnect();
	void SSLProcessingAccept();

	void SSLProcessingSend();
	void SSLProcessingRecv();

	
	void CreateServerSSLContext();
	void CreateClientSSLContext();
};



class SSLServer : public TCPServer
{
public:
	using TCPServer::TCPServer;		
private:		
	virtual void AcceptNext() override;	
	virtual void Complition(DWORD dwBytesTransfered, OVERLAPPED* overlapped) override;
};












class UDPDomain;

class UDPTransport : public Transport
{
public:
	UDPTransport(IOCPBase* iocpBase, UDPDomain* udpDomain);

	UDPDomain* m_MainTransport;		

	struct sockaddr_in	m_SendAddress;
	INT					m_SendAddressLen;
	
	void Send(char* data, ULONG size) override;	

	virtual BYTE* GetRecvBuffer() override;
	virtual void Complition(DWORD dwBytesTransfered, OVERLAPPED* overlapped) override;
	
};


class UDPDomain :  public Socketable
{
public:
	typedef uint64_t UDPAssociatedAddress;
	static UDPAssociatedAddress MakeAssociatedAddress(const struct sockaddr_in& address);

	unordered_map<UDPAssociatedAddress, UDPTransport*> m_sockaddr2UDPTransports;

	void SendBroadcast(char* data, ULONG size);
	void Bind(const char* port);

	OVERLAPPED m_ReciveOverlapped;

	struct sockaddr_in	m_RecvAddress;
	INT					m_RecvAddressLen;	

	void StartReceiving();

	virtual void Complition(DWORD dwBytesTransfered, OVERLAPPED* overlapped) override;
	void OnRecv(DWORD dwBytesTransfered);
	void Remove(UDPTransport* transport);

	UDPDomain(IOCPBase* iocpBase);
private:
	static const struct sockaddr_in scm_BroadcastAddress;


friend class UDPTransport;
	vector<BYTE> m_RecvBuffer;
};



