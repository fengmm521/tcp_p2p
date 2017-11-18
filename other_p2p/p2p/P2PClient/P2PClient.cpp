//////////////////////////////////////////////////////
// p2pclient.cpp�ļ�


#include "p2pclient.h"

CP2PClient::CP2PClient()
{	
    m_bLogin = false;
    m_hThread = 0;
    m_hHeartBeatThread = 0;
    m_nServerSocket = INVALID_SOCKET;    
    m_nHeartBeatTime = 0;
}

CP2PClient::~CP2PClient()
{
    Logout();

    // ֪ͨ�����߳��˳�
    if (m_hHeartBeatThread != 0)
    {
        m_bHeartBeatThreadExit = false;
    }
    if(m_hThread != 0)
    {
        m_bThreadExit = true;
    }

    if(m_nServerSocket != INVALID_SOCKET)
        close(m_nServerSocket);

}

bool CP2PClient::Init(unsigned short usLocalPort)
{
    if(m_nServerSocket != INVALID_SOCKET)
        return false;

    m_nServerSocket = socket(AF_INET,SOCK_DGRAM,0);

    if(m_nServerSocket == SOCKET_ERROR)
    {  
        printf("socket()error\n");  
        return false;  
    }


    sockaddr_in localAddr = { 0 };
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(usLocalPort);
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(m_nServerSocket, (struct sockaddr *)&localAddr, sizeof(localAddr)) == SOCKET_ERROR)
    {
        close(m_nServerSocket);
        m_nServerSocket = INVALID_SOCKET;
        return false;
    }
    if(usLocalPort == 0)
    {
        int nLen = sizeof(localAddr);
        getsockname(m_nServerSocket, (sockaddr*)&localAddr, (socklen_t*)&nLen);
        usLocalPort = ntohs(localAddr.sin_port);
    }

    // ��ȡ���ػ�����IP��ַ���õ���ǰ�û���˽���ն�
    char szHost[256];
    gethostname(szHost, 256);
    hostent *pHost = gethostbyname(szHost);

    memset(&m_LocalPeer, 0, sizeof(m_LocalPeer));
    for(int i=0; i<MAX_ADDR_NUMBER - 1; i++)
    {
        char *p = pHost->h_addr_list[i];
        if(p == NULL)
            break;

        memcpy(&m_LocalPeer.addr[i].dwIp, &p, pHost->h_length);
        m_LocalPeer.addr[i].nPort = usLocalPort;
        m_LocalPeer.AddrNum ++;
    }

    // �������շ����߳�
    m_bThreadExit = false;
    pthread_create(&m_hThread, NULL, RecvThreadProc, this); 


    m_bHeartBeatThreadExit = false;
    pthread_create(&m_hHeartBeatThread, NULL, HeartBeatThreadProc, this);

    return true;
}

bool CP2PClient::Login(char *pszUserName, char *pszServerIp)
{
    if(m_bLogin || strlen(pszUserName) > MAX_USERNAME - 1)
        return false;

    // �������
    m_dwServerIp = inet_addr(pszServerIp);
    strncpy(m_LocalPeer.szUserName, pszUserName, strlen(pszUserName));


    // ����������
    sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = m_dwServerIp; 
    serverAddr.sin_port = htons(SERVER_PORT);

    // ������ͱ��û���Ϣ
    CP2PMessage logMsg;
    logMsg.nMessageType = USERLOGIN;
    memcpy(&logMsg.peer, &m_LocalPeer, sizeof(PEER_INFO)); 

    for(int i=0; i<MAX_TRY_NUMBER; i++)
    {
        sendto(m_nServerSocket, (char*)&logMsg, sizeof(logMsg), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
        for(int j=0; j<10; j++)
        {
            if(m_bLogin)
                return true;
            sleep(1);
        }
    }
    return false;
}

void CP2PClient::Logout()
{
    if(m_bLogin)
    {
        // ���߷�����������Ҫ�뿪��		
        CP2PMessage logMsg;
        logMsg.nMessageType = USERLOGOUT;
        memcpy(&logMsg.peer, &m_LocalPeer, sizeof(PEER_INFO)); 

        sockaddr_in serverAddr = { 0 };
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = m_dwServerIp; 
        serverAddr.sin_port = htons(SERVER_PORT);

        sendto(m_nServerSocket, (char*)&logMsg, sizeof(logMsg), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
        m_bLogin = false;
    }
}

bool CP2PClient::SendText(char *pszUserName, char *pszText, int nTextLen)
{
    if(!m_bLogin || strlen(pszUserName) > MAX_USERNAME - 1 
        || nTextLen > MAX_PACKET_SIZE - sizeof(CP2PMessage))
        return false;

    // �������
    char sendBuf[MAX_PACKET_SIZE];
    CP2PMessage *pMsg = (CP2PMessage*)sendBuf;
    pMsg->nMessageType = P2PMESSAGE;
    memcpy(&pMsg->peer, &m_LocalPeer, sizeof(m_LocalPeer));
    memcpy((pMsg + 1), pszText, nTextLen);

    m_bMessageACK = false;
    for(int i=0; i<MAX_TRY_NUMBER; i++)
    {
        PEER_INFO *pInfo = m_PeerList.GetAPeer(pszUserName);
        if(pInfo == NULL)
            return false;

        // ����Է�P2P��ַ��Ϊ0������ͼ����ΪĿ�ĵ�ַ�������ݣ�
        // �������ʧ�ܣ�����Ϊ��P2P��ַ��Ч
        if(pInfo->p2pAddr.dwIp != 0) 
        {	
            sockaddr_in peerAddr = { 0 };
            peerAddr.sin_family = AF_INET;
            peerAddr.sin_addr.s_addr = pInfo->p2pAddr.dwIp;
            peerAddr.sin_port = htons(pInfo->p2pAddr.nPort);

            sendto(m_nServerSocket, sendBuf, 
                nTextLen + sizeof(CP2PMessage), 0, (sockaddr*)&peerAddr, sizeof(peerAddr));

            for(int j=0; j<10; j++)
            {
                if( m_bMessageACK)
                    return true;
                sleep(1);
            }
        }

        // ����򶴣�������������P2P��ַ
        pInfo->p2pAddr.dwIp = 0;	

        // �������
        char tmpBuf[sizeof(CP2PMessage) + MAX_USERNAME];
        CP2PMessage *p = (CP2PMessage *)tmpBuf;
        p->nMessageType = P2PCONNECT;
        memcpy(&p->peer, &m_LocalPeer, sizeof(m_LocalPeer));
        memcpy((char*)(p + 1), pszUserName, strlen(pszUserName) + 1);

        // ����ֱ�ӷ���Ŀ�꣬
        sockaddr_in peerAddr = { 0 };
        peerAddr.sin_family = AF_INET;
        for(int j=0; j<pInfo->AddrNum; j++)
        {
            peerAddr.sin_addr.s_addr = pInfo->addr[j].dwIp;
            peerAddr.sin_port = htons(pInfo->addr[j].nPort);
            sendto(m_nServerSocket, tmpBuf, sizeof(CP2PMessage), 0, (sockaddr*)&peerAddr, sizeof(peerAddr));
        }

        // Ȼ��ͨ��������ת��������Է����Լ���
        sockaddr_in serverAddr = { 0 };
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = m_dwServerIp; 
        serverAddr.sin_port = htons(SERVER_PORT);
        sendto(m_nServerSocket, tmpBuf, 
            sizeof(CP2PMessage) + MAX_USERNAME, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));

        // �ȴ��Է���P2PCONNECTACK��Ϣ
        for(int j=0; j<10; j++)
        {
            if(pInfo->p2pAddr.dwIp != 0)
                break;
            sleep(1);
        }	
    }
    return 0;
}


bool CP2PClient::HeartBeat( char *pszUserName, int nSecond )
{
    if(!m_bLogin || strlen(pszUserName) > MAX_USERNAME - 1 
        || nSecond <= 0)
        return false;
    pInfo = NULL;
    pInfo = m_PeerList.GetAPeer(pszUserName);
    if(pInfo == NULL)
        return false;
    return true;
}

bool CP2PClient::GetUserList()
{	
    // ��������ַ
    sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = m_dwServerIp; 
    serverAddr.sin_port = htons(SERVER_PORT);
    // �������
    CP2PMessage msgList;
    msgList.nMessageType = GETUSERLIST;	
    memcpy(&msgList.peer, &m_LocalPeer, sizeof(m_LocalPeer));
    // ɾ�����нڵ�
    if (true)
    {
        CAutoLock lock(&m_APILock);
        m_PeerList.DeleteAllPeers();
    }

    // ����GETUSERLIST���󣬵ȴ��б������
    m_bUserlistCmp = false;	
    int nUserCount = 0;
    for(int i=0; i<MAX_TRY_NUMBER; i++)
    {
        sendto(m_nServerSocket, (char*)&msgList, 
            sizeof(msgList), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
        do
        {	
            nUserCount = m_PeerList.m_nCurrentSize;
            for(int j=0; j<10; j++)
            {
                if(m_bUserlistCmp)
                    return true;
                sleep(1);
            }
        }while(m_PeerList.m_nCurrentSize > nUserCount);
    }
    return false;
}

void * CP2PClient::RecvThreadProc(void* lpParam)
{	
    //printf("succ\n");
    CP2PClient *pThis = (CP2PClient *)lpParam;	
    char buff[MAX_PACKET_SIZE+1];
    sockaddr_in remoteAddr;
    int nAddrLen = sizeof(remoteAddr);

    // ���մ���������Ϣ
    while(true)
    {
        unsigned long dwRecv, dwFlags = 0;


        dwRecv = recvfrom(pThis->m_nServerSocket,buff,MAX_PACKET_SIZE,0,(sockaddr*)&remoteAddr,(socklen_t*)&nAddrLen);
        // ���Ȳ鿴�Ƿ�Ҫ�˳�
        if(pThis->m_bThreadExit)
            break;
        // ����HandleIO���������������Ϣ
        pThis->HandleIO(buff, dwRecv, &remoteAddr, nAddrLen);
    }
    return 0;
}


void * CP2PClient::HeartBeatThreadProc( void* lpParam )
{
    CP2PClient *pThis = (CP2PClient *)lpParam;	
    //pThis->HeartBeatThread();
    return 0;
}

void CP2PClient::HandleIO(char *pBuf, int nBufLen, struct sockaddr_in *addr, int nAddrLen)
{
    CP2PMessage *pMsg = (CP2PMessage*)pBuf;
    if(nBufLen < sizeof(CP2PMessage))
        return;

    switch(pMsg->nMessageType)
    {
    case USERLOGACK:		// ���յ������������ĵ�½ȷ��
        {
            memcpy(&m_LocalPeer, &pMsg->peer, sizeof(PEER_INFO));
            m_bLogin = true;
        }
        break;
    case P2PMESSAGE:		// ��һ���ڵ������Ƿ�����Ϣ
        {
            int nDataLen = nBufLen - sizeof(CP2PMessage);
            if(nDataLen > 0)
            {
                // ����ȷ����Ϣ
                CP2PMessage ackMsg;
                ackMsg.nMessageType = P2PMESSAGEACK;
                memcpy(&ackMsg.peer, &m_LocalPeer, sizeof(PEER_INFO));
                sendto(m_nServerSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)addr, nAddrLen);		

                OnRecv(pMsg->peer.szUserName, (char*)(pMsg + 1), nDataLen);
            }
        }
        break;
    case P2PMESSAGEACK:		// �յ���Ϣ��Ӧ��
        {
            m_bMessageACK = true;
        }
        break;
    case P2PHEARTBEAT:		// ��һ���ڵ������Ƿ�����Ϣ
        {
            int nDataLen = nBufLen - sizeof(CP2PMessage);
            if(nDataLen > 0)
            {
                // ����ȷ����Ϣ
                CP2PMessage ackMsg;
                ackMsg.nMessageType = P2PHEARTBEATACK;
                memcpy(&ackMsg.peer, &m_LocalPeer, sizeof(PEER_INFO));
                sendto(m_nServerSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)addr, nAddrLen);		

                OnRecv(pMsg->peer.szUserName, (char*)(pMsg + 1), nDataLen);
            }
        }
        break;
    case P2PHEARTBEATACK:		// �յ���Ϣ��Ӧ��
        {
            m_bHeartBeatACK = true;
        }
        break;

    case P2PCONNECT:		// һ���ڵ�������P2P���ӣ��򶴣��������Ƿ����������ģ�Ҳ�����������ڵ㷢����
        {
            CP2PMessage ackMsg;
            ackMsg.nMessageType = P2PCONNECTACK;
            memcpy(&ackMsg.peer, &m_LocalPeer, sizeof(PEER_INFO));

            if(((sockaddr_in*)addr)->sin_addr.s_addr != m_dwServerIp)	// �ڵ㷢������Ϣ
            {
                {
                    CAutoLock lock(&m_APILock);
                    PEER_INFO *pInfo = m_PeerList.GetAPeer(pMsg->peer.szUserName);
                    if(pInfo != NULL)
                    {
                        if(pInfo->p2pAddr.dwIp == 0)
                        {
                            pInfo->p2pAddr.dwIp = ((sockaddr_in*)addr)->sin_addr.s_addr;
                            pInfo->p2pAddr.nPort = ntohs(((sockaddr_in*)addr)->sin_port);

                            printf(" Set P2P address for %s -> %s:%ld \n", pInfo->szUserName, 
                                inet_ntoa(((sockaddr_in*)addr)->sin_addr), ntohs(((sockaddr_in*)addr)->sin_port));
                        }
                    }
                }

                sendto(m_nServerSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)addr, nAddrLen);
            }
            else	// ������ת������Ϣ
            {
                // ��ڵ�������ն˷��ʹ���Ϣ
                sockaddr_in peerAddr = { 0 };
                peerAddr.sin_family = AF_INET;
                for(int i=0; i<pMsg->peer.AddrNum; i++)
                {
                    peerAddr.sin_addr.s_addr = pMsg->peer.addr[i].dwIp;
                    peerAddr.sin_port = htons(pMsg->peer.addr[i].nPort);
                    sendto(m_nServerSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)&peerAddr, sizeof(peerAddr));
                }
            }
        }
        break;

    case P2PCONNECTACK:		// ���յ��ڵ�Ĵ���Ϣ����������������P2Pͨ�ŵ�ַ
        {
            CAutoLock lock(&m_APILock);
            PEER_INFO *pInfo = m_PeerList.GetAPeer(pMsg->peer.szUserName);
            if(pInfo != NULL)
            {		
                if(pInfo->p2pAddr.dwIp == 0)
                {
                    pInfo->p2pAddr.dwIp = ((sockaddr_in*)addr)->sin_addr.s_addr;
                    pInfo->p2pAddr.nPort = ntohs(((sockaddr_in*)addr)->sin_port);

                    printf(" Set P2P address for %s -> %s:%ld \n", pInfo->szUserName, 
                        inet_ntoa(((sockaddr_in*)addr)->sin_addr), ntohs(((sockaddr_in*)addr)->sin_port));
                }

            }
        }
        break;

    case USERACTIVEQUERY:	// ������ѯ���Ƿ���
        {
            CP2PMessage ackMsg;
            ackMsg.nMessageType = USERACTIVEQUERYACK;
            memcpy(&ackMsg.peer, &m_LocalPeer, sizeof(PEER_INFO));
            sendto(m_nServerSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)addr, nAddrLen);
        }
        break;
    case GETUSERLIST:		// ���������͵��û��б�
        {	
            // ����������û���P2P��ַ���ٽ��û���Ϣ���浽�����û��б���
            pMsg->peer.p2pAddr.dwIp = 0;
            CAutoLock lock(&m_APILock);
            m_PeerList.AddAPeer(&pMsg->peer);
        }
        break;
    case USERLISTCMP:		// �û��б������
        {
            m_bUserlistCmp = true;
        }
        break;
    }
}

void CP2PClient::HeartBeatThread()
{
    while(!m_bHeartBeatThreadExit)
    {
        //if (m_nHeartBeatTime <= 0)
        //{
        //    Sleep(1);
        //}
        //// �������
        //m_nHeartBeatTime--;
        if (pInfo == NULL)
        {
            sleep(1);
            continue;
        }
        char* pszText = "HeartBeat";
        int nTextLen = 9;
        char sendBuf[MAX_PACKET_SIZE];
        CP2PMessage *pMsg = (CP2PMessage*)sendBuf;
        pMsg->nMessageType = P2PHEARTBEAT;
        memcpy(&pMsg->peer, &m_LocalPeer, sizeof(m_LocalPeer));
        memcpy((pMsg + 1), pszText, nTextLen);

        m_bHeartBeatACK = false;
        for(int i=0; i<MAX_TRY_NUMBER; i++)
        {
            // ����Է�P2P��ַ��Ϊ0������ͼ����ΪĿ�ĵ�ַ�������ݣ�
            // �������ʧ�ܣ�����Ϊ��P2P��ַ��Ч
            if(pInfo->p2pAddr.dwIp != 0) 
            {	
                sockaddr_in peerAddr = { 0 };
                peerAddr.sin_family = AF_INET;
                peerAddr.sin_addr.s_addr = pInfo->p2pAddr.dwIp;
                peerAddr.sin_port = htons(pInfo->p2pAddr.nPort);

                sendto(m_nServerSocket, sendBuf, 
                    nTextLen + sizeof(CP2PMessage), 0, (sockaddr*)&peerAddr, sizeof(peerAddr));

                for(int j=0; j<10; j++)
                {
                    if( m_bHeartBeatACK)                        
                    {
                        break;
                    }
                    sleep(1);
                }
                if( m_bHeartBeatACK)                        
                {
                    char* szUser;
                    strcpy(szUser, &pInfo->szUserName[0]);
                    printf(" HeartBeat to %s succ \n",szUser);
                    sleep(1);
                    break;
                }
            }
            sleep(1);
            //// ����򶴣�������������P2P��ַ
            //pInfo->p2pAddr.dwIp = 0;	

            //// �������
            //char tmpBuf[sizeof(CP2PMessage) + MAX_USERNAME];
            //CP2PMessage *p = (CP2PMessage *)tmpBuf;
            //p->nMessageType = P2PCONNECT;
            //memcpy(&p->peer, &m_LocalPeer, sizeof(m_LocalPeer));
            //memcpy((char*)(p + 1), pszUserName, strlen(pszUserName) + 1);

            //// ����ֱ�ӷ���Ŀ�꣬
            //sockaddr_in peerAddr = { 0 };
            //peerAddr.sin_family = AF_INET;
            //for(int j=0; j<pInfo->AddrNum; j++)
            //{
            //    peerAddr.sin_addr.s_addr = pInfo->addr[j].dwIp;
            //    peerAddr.sin_port = htons(pInfo->addr[j].nPort);
            //    sendto(m_nServerSocket, tmpBuf, sizeof(CP2PMessage), 0, (sockaddr*)&peerAddr, sizeof(peerAddr));
            //}

            //// Ȼ��ͨ��������ת��������Է����Լ���
            //sockaddr_in serverAddr = { 0 };
            //serverAddr.sin_family = AF_INET;
            //serverAddr.sin_addr.s_addr = m_dwServerIp; 
            //serverAddr.sin_port = htons(SERVER_PORT);
            //sendto(m_nServerSocket, tmpBuf, 
            //    sizeof(CP2PMessage) + MAX_USERNAME, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));

            //// �ȴ��Է���P2PCONNECTACK��Ϣ
            //for(int j=0; j<10; j++)
            //{
            //    if(pInfo->p2pAddr.dwIp != 0)
            //        break;
            //    sleep(1);
            //}	
        }
    }
    printf(" HeartBeat thread stop succ \n");
    return ;
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


//main����

int Run (char szServerIp[20],char szUserName[MAX_USERNAME])
{
    CMyP2P *client =new CMyP2P();
    if(!client->Init(0))
    {
        printf(" CP2PClient::Init() failed \n");
        printf("succ\n");
        return -1;
    }

    // ��½������
    if(!client->Login(szUserName, szServerIp))
    {
        printf(" CP2PClient::Login() failed \n");
        return -2;
    }
    // ��һ�ε�½�����ȸ����û��б�
    if(client->GetUserList())
    {
        printf(" Have %d users logined server: \n", client->m_PeerList.m_nCurrentSize);
        for(int i=0; i<client->m_PeerList.m_nCurrentSize; i++)
        {
            PEER_INFO *pInfo = &client->m_PeerList.m_pPeer[i];
            printf(" Username: %s(%s:%ld) \n", pInfo->szUserName, 
                inet_ntoa(*((in_addr*)&pInfo->addr[pInfo->AddrNum -1].dwIp)), pInfo->addr[pInfo->AddrNum - 1].nPort);
        }
    }
    else
    {
        printf(" Get User List Failure !\n");
    }
    // ����ǰ״̬�ͱ�������÷�������û�
    printf(" %s has successfully logined server \n", szUserName);
    printf("\n Commands are: \"getu\", \"send\", \"beat\",\"help\",\"exit\" \n");
    // ѭ�������û�����
    char szCommandLine[256];
    while(true)
    {
        fgets(szCommandLine, 255, stdin);
        if(strlen(szCommandLine) < 4)
            continue;

        // ����������
        char szCommand[10];
        strncpy(szCommand, szCommandLine, 4);
        szCommand[4] = '\0';
        if(strcasecmp(szCommand, "getu") == 0)
        {
            // ��ȡ�û��б�
            if(client->GetUserList())
            {
                printf(" Have %d users logined server: \n", client->m_PeerList.m_nCurrentSize);
                for(int i=0; i<client->m_PeerList.m_nCurrentSize; i++)
                {
                    PEER_INFO *pInfo = &client->m_PeerList.m_pPeer[i];
                    printf(" Username: %s(%s:%ld) \n", pInfo->szUserName, 
                        inet_ntoa(*((in_addr*)&pInfo->addr[pInfo->AddrNum -1].dwIp)), pInfo->addr[pInfo->AddrNum - 1].nPort);
                }
            }
            else
            {
                printf(" Get User List Failure !\n");
            }
        }

        else if(strcasecmp(szCommand, "send") == 0)
        {
            // �������Է��û���
            char szPeer[MAX_USERNAME];
            int i = 0;
            for(i=5;;i++)
            {
                if(szCommandLine[i] != ' ')
                    szPeer[i-5] = szCommandLine[i];
                else
                {
                    szPeer[i-5] = '\0';
                    break;
                }	
            }

            // ������Ҫ���͵���Ϣ
            char szMsg[56];
            strcpy(szMsg, &szCommandLine[i+1]);

            // ������Ϣ
            if(client->SendText(szPeer, szMsg, strlen(szMsg)))
                printf(" Send OK! \n");
            else
                printf(" Send Failure! \n");

        }
        else if(strcasecmp(szCommand, "beat") == 0)
        {
            // �������Է��û���
            char szPeer[MAX_USERNAME];
            int i = 0;
            for(i=5;;i++)
            {
                if(szCommandLine[i] != ' ')
                    szPeer[i-5] = szCommandLine[i];
                else
                {
                    szPeer[i-5] = '\0';
                    break;
                }	
            }
            char* szUser;
            strcpy(szUser, &szPeer[0]);
            // ������Ҫ������ʱ�䳤��

            int nSecond = 0;
            char szMsg[56];
            strcpy(szMsg, &szCommandLine[i+1]);
            nSecond = atoi(szMsg);
            printf("%d\n",nSecond);

            // ������Ϣ
            if(client->HeartBeat(szPeer, nSecond))
                printf(" HeartBeat to %s start succ \n",szUser);
            else
                printf(" HeartBeat to %s start err \n",szUser);
        }
        else if(strcasecmp(szCommand, "help") == 0)
        {
            printf("\n Commands are: \"getu\", \"send\", \"beat\",\"help\",\"exit\" \n");
        }
        else if(strcasecmp(szCommand, "exit") == 0)
        {
            break;
        }
        else
        {
            printf("Command Unknown\n");
        }
    }
    return 0;
}

int main(int argc, char *argv[]) 
{	
    int nRetCode = 0;/*
    char *pServerAddess;
    char *pUserName;*/
    char szServerIp[20];
    char szUserName[MAX_USERNAME];
    if ( argc != 3 )
    {
        //// ��ȡ������IP��ַ���û���
        //printf(" Please input server ip: ");
        //gets(szServerIp);
        //printf(" Please input your name: ");
        //gets(szUserName);
        printf ( "Usage: TcpHoleClt.exe 192.168.1.17 zhhf\n" );
        return 0;
    }
    else
    {/*
        pServerAddess = argv[1];
        pUserName = argv[2];*/
        strcpy(szServerIp,(char*)argv[1]);
        strcpy(szUserName,(char*)argv[2]);
    }
    nRetCode = Run(szServerIp,szUserName);
	return nRetCode;
}


