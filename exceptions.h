#pragma once



class DebugException : public std::exception
{
public:
	char	m_Message[512];
	virtual const char* what() const noexcept;
	DebugException(char* file,  int line);	
};


#define SYS_EXCEPTION DebugException(__FILE__, __LINE__)
#define SYS_EXCEPTION_TYPE DebugException

#ifdef _DEBUG
#define ASSERT(cond) if (!(cond)) throw SYS_EXCEPTION
#else
#define ASSERT(cond) 
#endif
