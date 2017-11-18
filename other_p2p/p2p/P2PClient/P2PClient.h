#include<stdio.h>  
#include<stdlib.h>  
#include<unistd.h>  
#include<string.h>  
#include<sys/types.h>  
#include<sys/socket.h>  
#include<netinet/in.h>  
#include <arpa/inet.h>
#include<netdb.h> 
#include "../comm.h"
#include <pthread.h>
#include "../MyThread.h"

#define SOCKET_ERROR            -1
#define INVALID_SOCKET          0



class CPeerList
{
public:
    CPeerList();
    ~CPeerList();

    // ���б������һ���ڵ�
    bool AddAPeer(PEER_INFO *pPeer);
    // ����ָ���û�����Ӧ�Ľڵ�
    PEER_INFO *GetAPeer(char *pszUserName);
    // ���б���ɾ��һ���ڵ�
    void DeleteAPeer(char *pszUserName);
    // ɾ�����нڵ�
    void DeleteAllPeers();

    // ��ͷָ��ͱ�Ĵ�С
    PEER_INFO *m_pPeer;	
    int m_nCurrentSize;

protected:
    int m_nTatolSize;	
};

class CP2PClient
{
public:
    CP2PClient();
    ~CP2PClient();
    // ��ʼ������ĳ�Ա
    bool Init(unsigned short usLocalPort = 0);

    // ��½���������ǳ�������
    bool Login(char *pszUserName, char *pszServerIp);
    void Logout();

    // ������������û��б������û��б��¼
    bool GetUserList();

    // ��һ���û�������Ϣ
    bool SendText(char *pszUserName, char *pszText, int nTextLen);

    // ��һ���û���������
    bool HeartBeat(char *pszUserName, int nSecond);

    // ���յ�����Ϣ���麯��
    virtual void OnRecv(char *pszUserName, char *pszData, int nDataLen) { }

    // �û��б�
    CPeerList m_PeerList;

protected:
    void HandleIO(char *pBuf, int nBufLen, struct sockaddr_in *addr, int nAddrLen);
    static void *  RecvThreadProc(void* lpParam);
    void HeartBeatThread();
    static void *  HeartBeatThreadProc(void* lpParam);

    CCritSec	m_APILock;              // ͬ�����û��б�ķ���
    

    int m_nServerSocket;				// ����P2Pͨ�ŵ��׽��־��
    
    pthread_t m_hThread;		// �߳̾��	

    PEER_INFO m_LocalPeer;	// ���û���Ϣ

    unsigned long m_dwServerIp;		// ������IP��ַ

    bool m_bThreadExit;		// ����ָʾ�����߳��˳�

    bool m_bHeartBeatThreadExit;		// ���������߳��˳�
    pthread_t m_hHeartBeatThread;		// �����߳̾��	


    bool m_bLogin;			// �Ƿ��½
    bool m_bUserlistCmp;	// �û��б��Ƿ������
    bool m_bMessageACK;		// �Ƿ���յ���ϢӦ��
    bool m_bHeartBeatACK;	// �Ƿ���յ���ϢӦ��

    int m_nHeartBeatTime;
    PEER_INFO *pInfo;
};


class CMyP2P : public CP2PClient
{
public:

    void OnRecv(char *pszUserName, char *pszData, int nDataLen)
    {
        pszData[nDataLen] = '\0';
        printf(" Recv a Message from %s :  %s \n", pszUserName, pszData);
    }
};


