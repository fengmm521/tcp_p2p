/******************************************************************************
* FILE:	MyThread.cpp
* Description:	
*	Implementation of the CCritSec, CAutoLock and CMyThread class.
*
* Modified Code History
* Mark	Date		By			Modification Reason
*******************************************************************************
* 01	2007-1-29	Yin-XiaoGui	Initial creation.
******************************************************************************/

#include <stdio.h>
#include <signal.h>
#include "MyThread.h"
 #include <unistd.h>

CMyThread::CMyThread()
{
	m_nThreadID = 0;
	m_bThreadStarted = false;
	ResetStopEvent();
}

CMyThread::~CMyThread()
{
 	//should we call StopThread here ??
}

int CMyThread::StartThread(int policy, int Priority, int StackSize)
{
	int iRet;
	CAutoLock lock(&m_APILock);
	
	if (m_bThreadStarted)
	{	
		return -1;
	}
		
	ResetStopEvent();

	//Set up POSIX thread attributes
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	
	//set thread's policy
	iRet = pthread_attr_setschedpolicy(&attr, policy);
	if (iRet != 0)
	{
		perror("Thread set policy failure!");
	}
	
	//set thread's priority
	sched_param param;
	param.sched_priority = Priority;
	iRet = pthread_attr_setschedparam(&attr, &param);
	if (iRet != 0)
	{
		perror("Thread set prior failure!");
	}
	
	//set thread's stack size
	iRet = pthread_attr_setstacksize(&attr, StackSize);
	if (iRet != 0)
	{
		perror("Thread set stack size failure!");
	}
	
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	
	//startup thread to run
	int nRet = pthread_create(&m_nThreadID, &attr, CMyThread::ThreadEntryPoint, this);
	if (nRet != 0)
	{
		// process error here
		
		pthread_attr_destroy(&attr);
		return -1;
	}
	pthread_attr_destroy(&attr);
	
	m_bThreadStarted = true;
	
	return 0;	
}

int CMyThread::StopThread(unsigned long timeout)
{
	if (!m_bThreadStarted)
	{
		return 0;  //this should return 0
	}

	CAutoLock lock(&m_APILock);
	SetStopEvent();

	//TODO: pthread_join does not support timeout, need to implement a timer
	int nRet = pthread_join(m_nThreadID,NULL);
	if (nRet)
	{
		// handle error here
		return -1;
	}
	
	m_bThreadStarted = false;
	
	return 0;
}

// kill the thread immediately, not implemented yet
int CMyThread::KillThread()
{
	pthread_kill(m_nThreadID, 0);
	
	return 0;
}

int CMyThread::CheckStopEvent()
{
	return GetStopEvent();
}

void *CMyThread::ThreadEntryPoint(void *pParam)
{
	CMyThread *obj = static_cast<CMyThread *>(pParam);
	
	obj->ThreadProc();
	//((CMyThread *)pParam)->ThreadProc();
	
	return NULL;
}
