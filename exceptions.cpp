#include "Exceptions.h"


const char* DebugException::what() const noexcept
{
	return m_Message;
}

DebugException::DebugException(char* file, int line)
{
	ZeroMemory(m_Message, sizeof(m_Message));

	auto len = strlen(file);

	strcpy_s(m_Message, sizeof(m_Message), file);

	auto offset = std::min<decltype(len)>(sizeof(m_Message) / sizeof(char) - 8, len);

	_itoa_s(line, m_Message + offset + 1, 7, 10);

	m_Message[offset] = ':';
}

