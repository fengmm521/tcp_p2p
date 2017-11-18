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

    // 向列表中添加一个节点
    bool AddAPeer(PEER_INFO *pPeer);
    // 查找指定用户名对应的节点
    PEER_INFO *GetAPeer(char *pszUserName);
    // 从列表中删除一个节点
    void DeleteAPeer(char *pszUserName);
    // 删除所有节点
    void DeleteAllPeers();

    // 表头指针和表的大小
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
    // 初始化对象的成员
    bool Init(unsigned short usLocalPort = 0);

    // 登陆服务器，登出服务器
    bool Login(char *pszUserName, char *pszServerIp);
    void Logout();

    // 向服务器请求用户列表，更新用户列表记录
    bool GetUserList();

    // 向一个用户发送消息
    bool SendText(char *pszUserName, char *pszText, int nTextLen);

    // 向一个用户发送心跳
    bool HeartBeat(char *pszUserName, int nSecond);

    // 接收到来消息的虚函数
    virtual void OnRecv(char *pszUserName, char *pszData, int nDataLen) { }

    // 用户列表
    CPeerList m_PeerList;

protected:
    void HandleIO(char *pBuf, int nBufLen, struct sockaddr_in *addr, int nAddrLen);
    static void *  RecvThreadProc(void* lpParam);
    void HeartBeatThread();
    static void *  HeartBeatThreadProc(void* lpParam);

    CCritSec	m_APILock;              // 同步对用户列表的访问
    

    int m_nServerSocket;				// 用于P2P通信的套节字句柄
    
    pthread_t m_hThread;		// 线程句柄	

    PEER_INFO m_LocalPeer;	// 本用户信息

    unsigned long m_dwServerIp;		// 服务器IP地址

    bool m_bThreadExit;		// 用于指示接收线程退出

    bool m_bHeartBeatThreadExit;		// 用于心跳线程退出
    pthread_t m_hHeartBeatThread;		// 心跳线程句柄	


    bool m_bLogin;			// 是否登陆
    bool m_bUserlistCmp;	// 用户列表是否传输结束
    bool m_bMessageACK;		// 是否接收到消息应答
    bool m_bHeartBeatACK;	// 是否接收到消息应答

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


