///////////////////////////////////////
// P2PServer.cpp文件
#include "P2PServer.h"

static void * IOThreadProc(void * lpParam);

CPeerList  g_PeerList;				// 客户列表
int g_s;							// UDP套节字

CCritSec	m_APILock;              // 同步对用户列表的访问
pthread_t   m_gThread;		        // 线程句柄	

unsigned long GetTickCount()
{
    unsigned long currentTime;
    struct timeval current;
    gettimeofday(&current, NULL);
    currentTime = current.tv_sec * 1000 + current.tv_usec/1000;
    return currentTime;
}

int main(int argc, char *argv[])
{
	// 创建套节字，绑定到本地端口
	g_s = socket(AF_INET,SOCK_DGRAM,0);
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(SERVER_PORT);
	sin.sin_addr.s_addr = INADDR_ANY;
	if(bind(g_s, (struct sockaddr *)&sin, sizeof(sin)) == SOCKET_ERROR)
	{
		printf(" bind() failed \n");
		return -1;
	}

	///////////////////////////////////////////////////////
    // 下面这段代码用来显示服务器绑定的终端
    char szHost[256];
	gethostname(szHost, 256);
	hostent *pHost = gethostbyname(szHost);
	in_addr addr;
	for(int i = 0; ; i++)
	{
		char *p = pHost->h_addr_list[i];
		if(p == NULL)
			break;
		
		memcpy(&addr.s_addr, p, pHost->h_length);
		printf(" bind to local address -> %s:%ld \n", inet_ntoa(addr), SERVER_PORT);
	}	
	
	// 开启服务	
	printf(" P2P Server starting... \n\n");

    pthread_create(&m_gThread, NULL, IOThreadProc, NULL);  

	// 定时向客户方发送“询问”消息，删除不响应的用户
	while(true)
    {
        CP2PMessage queryMsg;
        queryMsg.nMessageType = USERACTIVEQUERY;
        unsigned long dwTick = GetTickCount();
        for(int i=0; i<g_PeerList.m_nCurrentSize; i++)
        {
            if(false)/*(dwTick - g_PeerList.m_pPeer[i].dwLastActiveTime >= 2*15*1000 + 600) */
            {
                printf(" delete a non-active user: %s \n", g_PeerList.m_pPeer[i].szUserName);
                {
                    CAutoLock lock(&m_APILock);
                    g_PeerList.DeleteAPeer(g_PeerList.m_pPeer[i].szUserName);
                }
                // 因为删了当前遍历到的用户，所以i值就不应该加1了
                i--;
            }
            else
            {
                // 注意，地址列表中的最后一个地址是客户的公共地址，询问消息应该发向这个地址
                sockaddr_in peerAddr = { 0 };
                peerAddr.sin_family = AF_INET;
                peerAddr.sin_addr.s_addr = 
                    g_PeerList.m_pPeer[i].addr[g_PeerList.m_pPeer[i].AddrNum - 1].dwIp;
                peerAddr.sin_port = 
                    htons(g_PeerList.m_pPeer[i].addr[g_PeerList.m_pPeer[i].AddrNum - 1].nPort);

                sendto(g_s, (char*)&queryMsg, sizeof(queryMsg), 0, (sockaddr*)&peerAddr, sizeof(peerAddr));
            }
        }
        sleep(5);
	}

	printf(" P2P Server shutdown. \n");
	close(m_gThread);
	close(g_s);
    return 0;
}


void * IOThreadProc(void * lpParam)
{
	char buff[MAX_PACKET_SIZE+1];
	CP2PMessage *pMsg = (CP2PMessage*)buff;
	sockaddr_in remoteAddr;
	int nRecv, nAddrLen = sizeof(remoteAddr);
	while(true)
	{
		nRecv = recvfrom(g_s, buff, MAX_PACKET_SIZE, 0, (sockaddr*)&remoteAddr, (socklen_t*)&nAddrLen);
		if(nRecv == SOCKET_ERROR)
		{
			printf(" recvfrom() failed \n");
			continue;
		}
		if(nRecv < sizeof(CP2PMessage))
			continue;

		// 防止用户发送错误的用户名
		pMsg->peer.szUserName[MAX_USERNAME] = '\0'; 
		switch(pMsg->nMessageType)
		{
		case USERLOGIN:		// 有用户登陆			
			{
				// 设置用户的公共终端信息，记录用户的活动时间
				pMsg->peer.addr[pMsg->peer.AddrNum].dwIp = remoteAddr.sin_addr.s_addr;
				pMsg->peer.addr[pMsg->peer.AddrNum].nPort = ntohs(remoteAddr.sin_port);
                pMsg->peer.AddrNum ++;
                pMsg->peer.dwLastActiveTime = GetTickCount();

                bool bOK;
				// 将用户信息保存到用户列表中
                {
                    CAutoLock lock(&m_APILock);
                    bOK = g_PeerList.AddAPeer(&pMsg->peer);
                }
				if(bOK)
				{
					// 发送确认消息，将用户的公共地址传递过去
					pMsg->nMessageType = USERLOGACK;
					sendto(g_s, (char*)pMsg, sizeof(CP2PMessage), 0, (sockaddr*)&remoteAddr, sizeof(remoteAddr));	
					
					printf(" has a user login : %s (%s:%ld) \n", 
						pMsg->peer.szUserName, inet_ntoa(remoteAddr.sin_addr), ntohs(remoteAddr.sin_port));
				}
			}
			break;
		case USERLOGOUT:	// 有用户登出
			{
                {
                    CAutoLock lock(&m_APILock);
                    g_PeerList.DeleteAPeer(pMsg->peer.szUserName);
                }
				
				printf(" has a user logout : %s (%s:%ld) \n", 
					pMsg->peer.szUserName, ::inet_ntoa(remoteAddr.sin_addr), ntohs(remoteAddr.sin_port));
			}
			break;

		case GETUSERLIST:	// 有用户请求发送用户列表
			{
				printf(" sending user list information to %s (%s:%ld)... \n",
					pMsg->peer.szUserName, ::inet_ntoa(remoteAddr.sin_addr), ntohs(remoteAddr.sin_port));
				CP2PMessage peerMsg;
				peerMsg.nMessageType = GETUSERLIST;
				for(int i=0; i<g_PeerList.m_nCurrentSize; i++)
				{
					memcpy(&peerMsg.peer, &g_PeerList.m_pPeer[i], sizeof(PEER_INFO));
					sendto(g_s, (char*)&peerMsg, sizeof(CP2PMessage), 0, (sockaddr*)&remoteAddr, sizeof(remoteAddr));
				}

				// 发送结束封包
				peerMsg.nMessageType = USERLISTCMP;
				sendto(g_s, (char*)&peerMsg, sizeof(CP2PMessage), 0, (sockaddr*)&remoteAddr, sizeof(remoteAddr));
			}
			break;
		case P2PCONNECT:	// 有用户请求让另一个用户向它发送打洞消息
			{				
				char *pszUser = (char*)(pMsg + 1);
				printf(" %s wants to connect to %s \n", pMsg->peer.szUserName, pszUser);
                PEER_INFO *pInfo;
                {
                    CAutoLock lock(&m_APILock);
                    pInfo = g_PeerList.GetAPeer(pszUser);
                }

				if(pInfo != NULL)
				{
					remoteAddr.sin_addr.s_addr = pInfo->addr[pInfo->AddrNum -1].dwIp;
					remoteAddr.sin_port = htons(pInfo->addr[pInfo->AddrNum -1].nPort);
					
					sendto(g_s, (char*)pMsg, sizeof(CP2PMessage), 0, (sockaddr*)&remoteAddr, sizeof(remoteAddr));	
				}
			}
			break;

		case USERACTIVEQUERYACK:	// 用户对“询问”消息的应答	
			{
				printf(" recv active ack message from %s (%s:%ld) \n",
					pMsg->peer.szUserName, ::inet_ntoa(remoteAddr.sin_addr), ntohs(remoteAddr.sin_port));
                {
                    CAutoLock lock(&m_APILock);
                    PEER_INFO *pInfo = g_PeerList.GetAPeer(pMsg->peer.szUserName);
                    if(pInfo != NULL)
                    {
                        pInfo->dwLastActiveTime = GetTickCount();
                    }
                }
			}
			break;
		}	
	}
	return 0;
}



CPeerList::CPeerList()
{
    m_nCurrentSize = 0;
    m_nTatolSize = 100;
    m_pPeer = new PEER_INFO[m_nTatolSize];
}

CPeerList::~CPeerList()
{
    delete[] m_pPeer;
}

bool CPeerList::AddAPeer(PEER_INFO *pPeer)
{
    if(GetAPeer(pPeer->szUserName) != NULL)
        return false;
    // 申请空间
    if(m_nCurrentSize >= m_nTatolSize) // 已经用完？
    {
        PEER_INFO *pTmp = m_pPeer;
        m_nTatolSize = m_nTatolSize * 2;
        m_pPeer = new PEER_INFO[m_nTatolSize];
        memcpy(m_pPeer, pTmp, m_nCurrentSize);
        delete pTmp;
    }
    // 添加到表中
    memcpy(&m_pPeer[m_nCurrentSize ++], pPeer, sizeof(PEER_INFO));
    return true;
}

PEER_INFO *CPeerList::GetAPeer(char *pszUserName)
{
    for(int i=0; i<m_nCurrentSize; i++)
    {
        if(strcasecmp(m_pPeer[i].szUserName, pszUserName) == 0)
        {
            return &m_pPeer[i];
        }
    }
    return NULL;
}

void CPeerList::DeleteAPeer(char *pszUserName)
{
    for(int i=0; i<m_nCurrentSize; i++)
    {
        if(strcasecmp(m_pPeer[i].szUserName, pszUserName) == 0)
        {
            memcpy(&m_pPeer[i], &m_pPeer[i+1], (m_nCurrentSize - i - 1)*sizeof(PEER_INFO));
            m_nCurrentSize --;
            break;
        }
    }
}

void CPeerList::DeleteAllPeers()
{
    m_nCurrentSize = 0;
}
