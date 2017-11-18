///////////////////////////////////////
// P2PServer.cpp�ļ�
#include "P2PServer.h"

static void * IOThreadProc(void * lpParam);

CPeerList  g_PeerList;				// �ͻ��б�
int g_s;							// UDP�׽���

CCritSec	m_APILock;              // ͬ�����û��б�ķ���
pthread_t   m_gThread;		        // �߳̾��	

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
	// �����׽��֣��󶨵����ض˿�
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
    // ������δ���������ʾ�������󶨵��ն�
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
	
	// ��������	
	printf(" P2P Server starting... \n\n");

    pthread_create(&m_gThread, NULL, IOThreadProc, NULL);  

	// ��ʱ��ͻ������͡�ѯ�ʡ���Ϣ��ɾ������Ӧ���û�
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
                // ��Ϊɾ�˵�ǰ���������û�������iֵ�Ͳ�Ӧ�ü�1��
                i--;
            }
            else
            {
                // ע�⣬��ַ�б��е����һ����ַ�ǿͻ��Ĺ�����ַ��ѯ����ϢӦ�÷��������ַ
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

		// ��ֹ�û����ʹ�����û���
		pMsg->peer.szUserName[MAX_USERNAME] = '\0'; 
		switch(pMsg->nMessageType)
		{
		case USERLOGIN:		// ���û���½			
			{
				// �����û��Ĺ����ն���Ϣ����¼�û��Ļʱ��
				pMsg->peer.addr[pMsg->peer.AddrNum].dwIp = remoteAddr.sin_addr.s_addr;
				pMsg->peer.addr[pMsg->peer.AddrNum].nPort = ntohs(remoteAddr.sin_port);
                pMsg->peer.AddrNum ++;
                pMsg->peer.dwLastActiveTime = GetTickCount();

                bool bOK;
				// ���û���Ϣ���浽�û��б���
                {
                    CAutoLock lock(&m_APILock);
                    bOK = g_PeerList.AddAPeer(&pMsg->peer);
                }
				if(bOK)
				{
					// ����ȷ����Ϣ�����û��Ĺ�����ַ���ݹ�ȥ
					pMsg->nMessageType = USERLOGACK;
					sendto(g_s, (char*)pMsg, sizeof(CP2PMessage), 0, (sockaddr*)&remoteAddr, sizeof(remoteAddr));	
					
					printf(" has a user login : %s (%s:%ld) \n", 
						pMsg->peer.szUserName, inet_ntoa(remoteAddr.sin_addr), ntohs(remoteAddr.sin_port));
				}
			}
			break;
		case USERLOGOUT:	// ���û��ǳ�
			{
                {
                    CAutoLock lock(&m_APILock);
                    g_PeerList.DeleteAPeer(pMsg->peer.szUserName);
                }
				
				printf(" has a user logout : %s (%s:%ld) \n", 
					pMsg->peer.szUserName, ::inet_ntoa(remoteAddr.sin_addr), ntohs(remoteAddr.sin_port));
			}
			break;

		case GETUSERLIST:	// ���û��������û��б�
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

				// ���ͽ������
				peerMsg.nMessageType = USERLISTCMP;
				sendto(g_s, (char*)&peerMsg, sizeof(CP2PMessage), 0, (sockaddr*)&remoteAddr, sizeof(remoteAddr));
			}
			break;
		case P2PCONNECT:	// ���û���������һ���û��������ʹ���Ϣ
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

		case USERACTIVEQUERYACK:	// �û��ԡ�ѯ�ʡ���Ϣ��Ӧ��	
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
    // ����ռ�
    if(m_nCurrentSize >= m_nTatolSize) // �Ѿ����ꣿ
    {
        PEER_INFO *pTmp = m_pPeer;
        m_nTatolSize = m_nTatolSize * 2;
        m_pPeer = new PEER_INFO[m_nTatolSize];
        memcpy(m_pPeer, pTmp, m_nCurrentSize);
        delete pTmp;
    }
    // ��ӵ�����
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
