#include "exceptions.h"


DebugException::DebugException(char* file, int line)
{
	auto len = strlen(file);

	strcpy_s(m_Message, sizeof(m_Message), file);

	auto offset = std::min<decltype(len)>(sizeof(m_Message) / sizeof(char) - 7, len);

	_itoa_s(line, m_Message + offset, 7, 10);
}

