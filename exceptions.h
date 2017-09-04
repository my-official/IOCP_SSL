#pragma once


#if defined(UNICODE) || defined(_UNICODE)
#define	universal_string	std::wstring
#else
#define	universal_string std::string
#endif	



class RuntimeException
{
public:
	void TextLine(const char* text);
	void TextLine(const std::string& text);

	void TextLine(const wchar_t* text);
	void TextLine(const std::wstring& text);

	virtual const TCHAR* ErrorMessage();
	void OutputToStdErr();
protected:
	universal_string m_ErrorMessage;
};

void OutputToStdErr(const char* text);
void OutputToStdErr(const std::string& text);


#ifdef _DEBUG

class DebugException : public RuntimeException
{
public:
	template <class ExeptionT>
	static ExeptionT& DebugFileAndLine(ExeptionT& e, const char* file, int line)
	{
		e.SetFileAndLine(file, line);
		return e;
	}

	void SetFileAndLine(const char* file, int line);
};



typedef DebugException BaseException;

#define EXCEPTION(exceptionInstance)	DebugException::DebugFileAndLine(exceptionInstance, __FILE__,__LINE__)
#else

typedef RuntimeException BaseException;

#define EXCEPTION(exceptionInstance)	exceptionInstance
#endif




class CachedMessageExeption : public BaseException
{
public:
	virtual const TCHAR* ErrorMessage() override;
protected:
	bool m_MessageFormated = false;
	virtual void FormattedMessageLine() = 0;
};



class SystemException : public CachedMessageExeption
{
public:
	SystemException(DWORD errorCode = GetLastError());

	DWORD m_ErrorCode;
protected:
	static void WindowsFormatMessage(BaseException& e, DWORD errorCode);

	virtual void FormattedMessageLine() override;
};



class ErrnoException : public SystemException
{
public:
	ErrnoException();
	int m_Errno;

protected:
	virtual void FormattedMessageLine() override;
};


class WSAException : public SystemException
{
public:
	SOCKET m_Socket;
	int m_WsaErrorCode;

	WSAException(int wsaErrorCode = WSAGetLastError());
	WSAException(SOCKET socket, int wsaErrorCode = WSAGetLastError());
protected:
	virtual void FormattedMessageLine() override;
};


class SSLException : public CachedMessageExeption
{
public:
	SSL* m_Ssl;
	int m_Result;

	SSLException(SSL *ssl, int result);
protected:
	virtual void FormattedMessageLine() override;
};