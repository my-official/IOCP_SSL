#pragma once



class DebugException : public std::exception
{
public:
	char	m_Message[512];
	DebugException(char* file,  int line);
};


#define SYS_EXCEPTION DebugException(__FILE__, __LINE__);


