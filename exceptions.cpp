#include "stdafx.h"
#include "Exceptions.h"

static std::wstring AnsiCharToWideChar(const char* text)
{
	auto textLength = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0) - 1;

	assert(textLength && textLength == strlen(text));

	std::wstring outString;
	outString.resize(textLength + 1, L'\n');
	
	int result = MultiByteToWideChar(CP_ACP, 0, text, -1, &outString[0], textLength + 1);

	assert(result);
}

static std::wstring AnsiCharToWideChar(const std::string& text)
{
	return AnsiCharToWideChar(text.c_str());
}



void RuntimeException::TextLine(const char* text)
{
#if defined(UNICODE) || defined(_UNICODE)
	auto textLength = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0) - 1;

	assert(textLength && textLength == strlen(text));

	auto prevErrorMessageLength = m_ErrorMessage.length();
	m_ErrorMessage.resize(prevErrorMessageLength + textLength + 1, L'\n');

	wchar_t* data = &m_ErrorMessage[prevErrorMessageLength];

	int result = MultiByteToWideChar(CP_ACP, 0, text, -1, data, textLength + 1);

	assert(result);
#else
	m_ErrorMessage += text;
	m_ErrorMessage += "\n";
#endif
}

void RuntimeException::TextLine(const std::string& text)
{
#if defined(UNICODE) || defined(_UNICODE)		
	auto textLength = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, NULL, 0) - 1;

	assert(textLength && textLength == text.length());

	auto prevErrorMessageLength = m_ErrorMessage.length();
	m_ErrorMessage.resize(prevErrorMessageLength + textLength + 1, L'\n');

	wchar_t* data = &m_ErrorMessage[prevErrorMessageLength];

	int result = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, data, textLength + 1);

	assert(result);
#else
	m_ErrorMessage += text;
	m_ErrorMessage += "\n";
#endif
}

void RuntimeException::TextLine(const wchar_t* text)
{
#if defined(UNICODE) || defined(_UNICODE)		
	m_ErrorMessage += text;
	m_ErrorMessage += L"\n";
#else
	m_ErrorMessage += wstring_convert< codecvt<wchar_t, char, mbstate_t> >().to_bytes(text);
	m_ErrorMessage += "\n";
#endif
}

void RuntimeException::TextLine(const std::wstring& text)
{
#if defined(UNICODE) || defined(_UNICODE)		
	m_ErrorMessage += text;
	m_ErrorMessage += L"\n";
#else
	m_ErrorMessage += wstring_convert< codecvt<wchar_t, char, mbstate_t> >().to_bytes(text.c_str());
	m_ErrorMessage += "\n";
#endif
}


const TCHAR* RuntimeException::ErrorMessage()
{
	return m_ErrorMessage.c_str();
}

void RuntimeException::OutputToStdErr()
{
#if defined(UNICODE) || defined(_UNICODE)		
	wcerr << ErrorMessage();
#else
	cerr << ErrorMessage();
#endif
}

void OutputToStdErr(const char* text)
{
#if defined(UNICODE) || defined(_UNICODE)	
	wcerr << AnsiCharToWideChar(text);
#else
	cerr << text;
#endif
}

void OutputToStdErr(const std::string& text)
{
	OutputToStdErr(text.c_str());
}


#ifdef _DEBUG

void DebugException::SetFileAndLine(const char* file, int line)
{
	assert(file);

	TextLine(string(file) + ":" + to_string(line));
}

#endif

const TCHAR* CachedMessageExeption::ErrorMessage()
{
	if (!m_MessageFormated)
		FormattedMessageLine();
	return BaseException::ErrorMessage();
}


SystemException::SystemException(DWORD errorCode /*= GetLastError()*/) : m_ErrorCode(errorCode)
{

}


void SystemException::FormattedMessageLine()
{
	WindowsFormatMessage(*this, m_ErrorCode);
}


void SystemException::WindowsFormatMessage(BaseException& e, DWORD errorCode)
{
	LPTSTR  lpBuffer = NULL;

	DWORD  result = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpBuffer,
		0, NULL);

	if (result == 0)
	{
		if (lpBuffer)
		{
			LocalFree(lpBuffer);
		}
		return;
	}

	try
	{
		e.TextLine(lpBuffer);
	}
	catch (...)
	{
		LocalFree(lpBuffer);
		throw;
	}
}

ErrnoException::ErrnoException() : m_Errno(errno)
{

}

void ErrnoException::FormattedMessageLine()
{
	SystemException::FormattedMessageLine();
	TextLine("errno == " + to_string(m_Errno));
}


WSAException::WSAException(int wsaErrorCode /*= WSAGetLastError()*/) : m_Socket(INVALID_SOCKET), m_WsaErrorCode(wsaErrorCode)
{

}

WSAException::WSAException(SOCKET socket, int wsaErrorCode /*= WSAGetLastError()*/) : m_Socket(socket), m_WsaErrorCode(wsaErrorCode)
{

}


void WSAException::FormattedMessageLine()
{
	if (m_Socket != INVALID_SOCKET)
	{
		int socketError = 0;
		int socketErrorSize = sizeof(socketError);

		if (getsockopt(m_Socket, SOL_SOCKET, SO_ERROR, (char*)&socketError, &socketErrorSize) == 0)
		{
			TextLine(L"Socket–based error code == " + to_wstring(socketError));
		}
	}

	TextLine(L"WSAGetLastError == " + to_wstring(m_WsaErrorCode));
	WindowsFormatMessage(*this, m_WsaErrorCode);
}

SSLException::SSLException(SSL *ssl, int result) : m_Ssl(ssl), m_Result(result)
{

}

void SSLException::FormattedMessageLine()
{
	int error = SSL_get_error(m_Ssl, m_Result);
	if (SSL_ERROR_NONE != error)
	{
		char message[512] = { 0 };
		int error_log = error;
		
		while (SSL_ERROR_NONE != error_log)
		{
			ERR_error_string_n(error_log, message, _countof(message) );
			TextLine("OpenSSL error_code == " + to_string(error_log));
			TextLine("OpenSSL error_string == " + string(message, strnlen_s(message, _countof(message))) );
			error_log = ERR_get_error();
		}
		
	}
}
