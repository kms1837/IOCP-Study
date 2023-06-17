#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <list>
#include <vector>
#include <mutex>
#include <thread>

#pragma comment(lib,"ws2_32.lib")

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

const int MAX_CLIENT = 30;

using ADDR_INFO = struct addrinfo;

using SOCKET_INFO = struct socketInfo {
    SOCKET pSocket;
    HANDLE hEventHandle = nullptr;
    char strNickName[50];
    char strAddress[50];
    OVERLAPPED overlapped;
    WSABUF wsabuf;
};

std::vector<SOCKET_INFO> pClientList;
int iClientCount = 0;

int InitAddressInfo(WSADATA* pWSADataOut, ADDR_INFO** ppAddressOut);
void SetupListenSocket(ADDR_INFO* pAddressInfo, SOCKET* ppOut);
void AcceptClientSocket(SOCKET pListenSocket, SOCKET* ppOut);
void RunServer(SOCKET pListenSocket);
void EventServerThread(SOCKET pListenSocket);
void RunServerCommand();
void ShutdownServer(SOCKET pListenSocket);
void JoinClient(SOCKET pListenSocket);
void ReceiveClient(int iIndex);
void ExitClient(int iIndex);
void ReceiveThread(int iIndex);
void EventServerReady(SOCKET pListenSocket);
HANDLE IOCPReady(SOCKET pListenSocket);
void IOCPServerThread(SOCKET pListenSocket, HANDLE* hNewCompletingPort);
void IOThread(HANDLE hCompletingPort);

int main() {
    ADDR_INFO* pAddrInfo = nullptr;
    HANDLE hCompletingPort = nullptr;
    int iResult = 0;

    WSADATA wsaData;
    iResult = InitAddressInfo(&wsaData, &pAddrInfo);
    if (iResult != 0) {
        return 1;
    }

    SOCKET pListenSocket = INVALID_SOCKET;
    try {
        SetupListenSocket(pAddrInfo, &pListenSocket);
    }
    catch (int iException) {
        freeaddrinfo(pAddrInfo);
        WSACleanup();

        return 1;
    }

    freeaddrinfo(pAddrInfo);

    try {
        RunServer(pListenSocket);
        RunServerCommand();
        ShutdownServer(pListenSocket);
    }
    catch (int iException) {
        WSACleanup();

        return 1;
    }

    return 0;
}

int InitAddressInfo(WSADATA* pWSADataOut, ADDR_INFO** ppAddressOut) {
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), pWSADataOut);
    if (iResult != 0) {
        std::cout << "WSAStartup failed: " << iResult << "\n";
        return 1;
    }

    ADDR_INFO hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, ppAddressOut);
    if (iResult != 0) {
        std::cout << "getaddrinfo failed: " << iResult << "\n";
        WSACleanup();
        return 1;
    }

    return 0;
}

void SetupListenSocket(ADDR_INFO* pAddressInfo, SOCKET* ppOut) {
    *ppOut = INVALID_SOCKET;
    *ppOut = socket(pAddressInfo->ai_family, pAddressInfo->ai_socktype, pAddressInfo->ai_protocol);

    if (*ppOut == INVALID_SOCKET) {
        std::cout << "Error at socket(): " << WSAGetLastError() << "\n";
        throw 1;
    }

    // 소켓 바인딩
    int iResult = bind(*ppOut, pAddressInfo->ai_addr, static_cast<int>(pAddressInfo->ai_addrlen));
    if (iResult == SOCKET_ERROR) {
        std::cout << "bind failed with error: " << WSAGetLastError() << "\n";
        closesocket(*ppOut);
        throw 1;
    }

    // 소켓에서 수신 대기
    if (listen(*ppOut, SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "Listen failed with error: " << WSAGetLastError() << "\n";
        throw 1;
    }
}

void AcceptClientSocket(SOCKET pListenSocket, SOCKET* ppOut) {
    // 소켓에서 수신 대기
    if (listen(pListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "Listen failed with error: " << WSAGetLastError() << "\n";
        throw 1;
    }

    // Accept a client socket
    *ppOut = accept(pListenSocket, NULL, NULL);
    if (*ppOut == INVALID_SOCKET) {
        std::cout << "accept failed: " << WSAGetLastError() << "\n";
        throw 1;
    }
}

void RunServer(SOCKET pListenSocket) {
    //EventServerReady(pListenSocket);

    IOCPReady(pListenSocket);

    std::cout << "~ run server ~\n";
}

void EventServerThread(SOCKET pListenSocket) {
    WSAEVENT handles[MAX_CLIENT + 1];
    int iIndex = 0;
    WSANETWORKEVENTS runEvent;

    pClientList[iClientCount].hEventHandle = WSACreateEvent();
    pClientList[iClientCount].pSocket = pListenSocket;
    strcpy_s(pClientList[iClientCount].strNickName, "admin");

    WSAEventSelect(pListenSocket, pClientList[iClientCount].hEventHandle, FD_ACCEPT);
    ++iClientCount;

    while (true) {
        memset(&handles, 0, sizeof(handles));
        for (int i = 0; i < iClientCount; ++i) {
            handles[i] = pClientList[i].hEventHandle;
        }

        iIndex = WSAWaitForMultipleEvents(iClientCount, handles, false, INFINITE, false);
        if ((iIndex != WSA_WAIT_FAILED) && (iIndex != WSA_WAIT_TIMEOUT)) {
            WSAEnumNetworkEvents(pClientList[iIndex].pSocket, pClientList[iIndex].hEventHandle, &runEvent);

            if (runEvent.lNetworkEvents == FD_ACCEPT) {
                JoinClient(pListenSocket);
            }
            else if (runEvent.lNetworkEvents == FD_READ) {
                ReceiveClient(iIndex);
            }
            else if (runEvent.lNetworkEvents == FD_CLOSE) {
                ExitClient(iIndex);
            }
        }
    }
}

void RunServerCommand() {
    std::string command = "";

    while (true) {
        std::cin >> command;
        if (command.compare("exit") == 0)
            break;
    }
}

void ShutdownServer(SOCKET pListenSocket) {
    // cleanup
    for (auto& client : pClientList) {
        closesocket(client.pSocket);
        WSACloseEvent(client.hEventHandle);
    }

    closesocket(pListenSocket);
    WSACleanup();
}

void JoinClient(SOCKET pListenSocket) {
    SOCKADDR_IN joinAddress;

    int iAddressLength = sizeof(joinAddress);
    pClientList[iClientCount].pSocket = accept(pListenSocket, (SOCKADDR*)&joinAddress, &iAddressLength);
    pClientList[iClientCount].hEventHandle = WSACreateEvent();

    WSAEventSelect(pClientList[iClientCount].pSocket, pClientList[iClientCount].hEventHandle, FD_READ | FD_CLOSE);

    ++iClientCount;

    std::cout << "접속\n";
}

void ReceiveClient(int iIndex) {
    std::thread receiveWaitThread = std::thread(ReceiveThread, iIndex);
    receiveWaitThread.detach();
}

void ExitClient(int iIndex) {
    closesocket(pClientList[iIndex].pSocket);
    WSACloseEvent(pClientList[iIndex].hEventHandle);

    --iClientCount;

    std::cout << "퇴장\n";
}

void ReceiveThread(int iIndex) {
    char strRecvBuffer[MAXBYTE];

    int iRecvLen = recv(pClientList[iIndex].pSocket, strRecvBuffer, MAXBYTE, 0);
    if (iRecvLen > 0) {
        std::cout << "유저" << iIndex << " 메시지 받음 : " << strRecvBuffer << "\n";
        for (int i = 1; i < iClientCount; i++) {
            if (iIndex == i) {
                continue;
            }

            send(pClientList[i].pSocket, strRecvBuffer, MAXBYTE, 0);
        }
    };
}

void EventServerReady(SOCKET pListenSocket) {
    pClientList.reserve(MAX_CLIENT);

    for (int i = 0; i < MAX_CLIENT; ++i) {
        SOCKET_INFO newClient;
        ZeroMemory(&newClient, sizeof(newClient));

        pClientList.push_back(newClient);
    }

    std::thread serverThread = std::thread(EventServerThread, pListenSocket);
    serverThread.detach();
}

HANDLE IOCPReady(SOCKET pListenSocket) {
    SYSTEM_INFO systemInfo;
    HANDLE hNewCompletingPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    GetSystemInfo(&systemInfo);
    for (int i = 0; i < systemInfo.dwNumberOfProcessors; ++i) {
        std::thread newIOThread = std::thread(IOThread, hNewCompletingPort);
        newIOThread.detach();
    }

    std::thread serverThread = std::thread(IOCPServerThread, pListenSocket, &hNewCompletingPort);
    serverThread.detach();

    return hNewCompletingPort;
}

void IOCPServerThread(SOCKET pListenSocket, HANDLE* hNewCompletingPort) {
    SOCKADDR_IN joinAddress;

    DWORD dwFlags;
    DWORD dwReceiveBytes;
    int iAddressLength;

    while (true) {
        int iAddressLength = sizeof(joinAddress);
        pClientList[iClientCount].pSocket = accept(pListenSocket, (SOCKADDR*)&joinAddress, &iAddressLength);

        std::cout << "접속\n";

        // 소켓 IOCP등록
        if ((*hNewCompletingPort = CreateIoCompletionPort((HANDLE)pClientList[iClientCount].pSocket, *hNewCompletingPort, pClientList[iClientCount].pSocket, 0)) == NULL) {
            std::cout << "CreateIoCompletionPort Error" << "\n";
            return;
        }

        dwFlags = 0;
        dwReceiveBytes = MAXBYTE;

        if (WSARecv(pClientList[iClientCount].pSocket, &pClientList[iClientCount].wsabuf, 1, &dwReceiveBytes, &dwFlags, &pClientList[iClientCount].overlapped, NULL) == SOCKET_ERROR) {
            if (WSAGetLastError() == ERROR_IO_PENDING) {
                std::cout << "WSARecv Success" << "\n";
            }
            else {
                std::cout << "WRSRecv Error" << pClientList[iClientCount].pSocket << WSAGetLastError() << "\n";
                return;
            }
        }

        ++iClientCount;
    }
}

void IOThread(HANDLE hCompletingPort) {
    DWORD flags = 0;
    SOCKET_INFO* socketInfo;
    DWORD dwNumBytes;
    ULONG_PTR completionKey;
    LPOVERLAPPED pOverlapped;

    while (true) {
        GetQueuedCompletionStatus(hCompletingPort, &dwNumBytes, &completionKey, &pOverlapped, INFINITE);
        socketInfo = (SOCKET_INFO*)pOverlapped;
        if (dwNumBytes == 0) {
            closesocket(socketInfo->pSocket);
            //free(sInfo);
            continue;
        }

        memset(&(socketInfo->overlapped), 0x00, sizeof(OVERLAPPED));
        send(socketInfo->pSocket, socketInfo->wsabuf.buf, socketInfo->wsabuf.len, 0);
        WSARecv(socketInfo->pSocket, &socketInfo->wsabuf, 1, &dwNumBytes, &flags, &socketInfo->overlapped, NULL);
    }
}