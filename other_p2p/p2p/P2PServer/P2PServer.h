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
