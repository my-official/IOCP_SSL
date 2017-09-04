#include "stdafx.h"
#include "Threads.h"
#include "Exceptions.h"
#include "base.h"


unsigned __stdcall _threadMain(void* data)
{	
	Thread* thread = (Thread*)data;	
	return thread->threadMain();
}

void Thread::Quit()
{
	SendQuit();
	if (!Join())
	{
		throw EXCEPTION(SystemException());
	}
}

Thread::Thread() : m_AnException(false), m_ThreadHandle(0), m_Finished(true)
{
	static volatile unsigned int threadId = 0;
	m_ThreadName = to_string(threadId);
	InterlockedIncrement(&threadId);
}


Thread::~Thread()
{
	if (!m_Finished)
	{
		Quit();
		m_Finished = true;
	}
}

bool Thread::RunThread(Thread& thread)
{
	assert(thread.m_Finished);
	thread.m_Finished = false;

	uintptr_t threadHandle = _beginthreadex(NULL, 1024 * 1024 * 2, &_threadMain, &thread, 0, NULL);

	if (threadHandle != 0)
	{
		thread.m_ThreadHandle = threadHandle;
		return true;
	}
	else
	{
		thread.m_Finished = true;
		return false;
	}	
}

bool Thread::Join()
{
	DWORD result = WaitForSingleObject((HANDLE)m_ThreadHandle, INFINITE);
	return (result == WAIT_OBJECT_0);
}

void Thread::JoinSome(const vector<HANDLE>& handles)
{
	DWORD result = WaitForMultipleObjects(handles.size(), handles.data(), TRUE, 2000);
	//return (result == WAIT_OBJECT_0);
}

unsigned Thread::threadMain()
{
	m_Finished = false;
	WriteToConsole("Thread %s started", m_ThreadName.c_str());
	try
	{	
		ThreadMain();
		m_Finished = true;
		return 0;
	}	
	catch (RuntimeException& e)
	{
		//m_Exception = e;
		m_AnException = true;
		WriteToConsole("RuntimeException: %s", e.ErrorMessage());
	}
	catch (std::exception& e)
	{
		m_Exception = e;
		m_AnException = true;
		WriteToConsole("std::exception: %s", e.what());
	}
	catch (...)
	{		
		m_AnException = true;
		WriteToConsole("unknown exception");
	}

	m_Finished = true;
	return 1;
}
