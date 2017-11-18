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
#include <sys/time.h>
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
