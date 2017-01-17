#pragma once


class Thread
{
public:
	string m_ThreadName;
	bool m_AnException;
	exception m_Exception;
	uintptr_t m_ThreadHandle;
	
	void Quit();

	Thread();
	virtual ~Thread();	

	static bool RunThread(Thread& thread);

	template <typename InputIterator>
	static void QuitSome(InputIterator begin, InputIterator end);

protected:
	virtual void ThreadMain() = 0;
	virtual void SendQuit() = 0;	
private:
	
	bool m_Finished;
	
	bool Join();
	static void JoinSome(const vector<HANDLE>& handles);

	friend unsigned __stdcall _threadMain(void* data);
	unsigned threadMain();
	
};

//template <typename InputIterator>
//void Thread::QuitSome(InputIterator begin, InputIterator end)
//{
//	vector<HANDLE> handles;
//	for (auto it = begin; it != end; ++it)
//	{
//		handles.push_back((HANDLE)it->m_ThreadHandle);
//	}
//
//	for (auto it = begin; it != end; ++it)
//	{
//		it->SendQuit();
//	}
//
//	JoinSome(handles);
//}

template <typename InputIterator>
void Thread::QuitSome(InputIterator begin, InputIterator end)
{
	vector<HANDLE> handles;
	for (auto it = begin; it != end; ++it)
	{
		handles.push_back((HANDLE)(*it)->m_ThreadHandle);
	}

	for (auto it = begin; it != end; ++it)
	{
		(*it)->SendQuit();
	}

	JoinSome(handles);
}

//template <typename Type>
//class CriticalSection
//{
//public:
//	
//
//
//	Type
//
//	Proxy<Type>& Lock();	
//	
//	CriticalSection();
//	~CriticalSection();	
//};