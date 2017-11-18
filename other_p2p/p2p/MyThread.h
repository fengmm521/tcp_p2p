/******************************************************************************
* FILE:	MyThread.h
* Description:	
*	Critical Section, AutoLock mutex and thread controler Class.
*
* Modified Code History
* Mark	Date		By			Modification Reason
*******************************************************************************
* 01	2007-1-29	Yin-XiaoGui	Initial creation.
******************************************************************************/

#ifndef __MYTHREAD_H__
#define __MYTHREAD_H__

using namespace std;

#include <pthread.h>

//Class for critical section
class CCritSec
{
public:
	CCritSec()
	{
		pthread_mutex_init(&m_CritSec, NULL);
	}
	
	~CCritSec()
	{
		pthread_mutex_destroy(&m_CritSec);
	}
	
	void Lock()
	{
		pthread_mutex_lock(&m_CritSec);
	}
	
	void Unlock()
	{
		pthread_mutex_unlock(&m_CritSec);
	}

private:
	//make copy constructor and assignment operator inaccessible
	CCritSec(const CCritSec &refCritSec);
	CCritSec &operator =(const CCritSec &refCritSec);
	
	pthread_mutex_t m_CritSec;
};

// locks a critical section, and unlocks it automatically
// when the lock goes out of scope
class CAutoLock
{
public:
	CAutoLock(CCritSec *pLock)
	{
		m_pLock = pLock;
		m_pLock->Lock();
	}
	
	~CAutoLock()
	{
		m_pLock->Unlock();
	}

private:
	//make copy constructor and assignment operator inaccessible
	CAutoLock(const CAutoLock &refAutoLock);
	CAutoLock &operator =(const CAutoLock &refAutoLock);

	CCritSec *m_pLock;
};

class CAutoBoolSet
{
public:
	CAutoBoolSet(bool* pFlag)
	{
		pVal = pFlag;
		*pVal = true;
	}
	
	~CAutoBoolSet(void)
	{
		*pVal = false;
	}
	
private:
	bool* pVal;
};

#define THREAD_PRIORITY_NORMAL			0			//Note: linux priority: -128 ~ +128
#define THREAD_STACK_SIZE				(32 * 1024)
#define THREAD_PRIORITY_TIME_CRITICAL	15

//Class for control thread
class CMyThread  
{
public:
	CMyThread();
	virtual ~CMyThread();

	//user callable functions
	int StartThread(int policy = SCHED_FIFO, int Priority = THREAD_PRIORITY_NORMAL, int StackSize = 327680);
	int StopThread(unsigned long timeout = 1000);
	int KillThread();								//kill the thread immediately, not implemented yet
	
	pthread_t GetThreadID(void) {return m_nThreadID;}
	
protected:
	virtual int ThreadProc() = 0;					//Main Worker thread function, override
	int CheckStopEvent();
	int SetStopEvent()
    {
    	CAutoLock l(&m_EventLock);
    	m_hStop = 1;
    	return 0;
    }
	bool m_bError;

private:
    CCritSec m_EventLock;
    int m_hStop;
    int GetStopEvent()
    {
    	CAutoLock l(&m_EventLock);
    	return m_hStop;
    }
    
    int ResetStopEvent() { CAutoLock l(&m_EventLock); m_hStop=0; return 0; }

private:
	static void *ThreadEntryPoint(void * pParam); 	// call this->ThreadProc()
	
	pthread_t	m_nThreadID;
	bool		m_bThreadStarted;
	CCritSec	m_APILock;  // lock for the API calls
};

#endif	//#ifndef __MYTHREAD_H__
