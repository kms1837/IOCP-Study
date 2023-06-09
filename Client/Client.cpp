#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#pragma comment(lib,"ws2_32.lib")

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

using ADDR_INFO = struct addrinfo;

void ReceiveThread(SOCKET pConnectSocket);

void LogIn() {
    std::cout << "==================================" << "\n";
    std::cout << "         ! 채팅방 !" << "\n";
    std::cout << "==================================" << "\n";
    std::cout << "접속할 이름 입력: ";

    std::string userName;
    std::cin >> userName;
}

int InitAddressInfo(char* strNodeName, WSADATA* pWSADataOut, ADDR_INFO** ppAddressOut) {
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), pWSADataOut);
    if (iResult != 0) {
        std::cout << "WSAStartup failed: " << iResult << "\n";
        return 1;
    }

    ADDR_INFO hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo(strNodeName, DEFAULT_PORT, &hints, ppAddressOut);
    if (iResult != 0) {
        std::cout << "getaddrinfo failed: " << iResult << "\n";
        WSACleanup();
        return 1;
    }

    return 0;
}

void ConnectServer(ADDR_INFO* pAddressInfo, SOCKET* ppOut) {
    int iResult;
    ADDR_INFO* ptr = nullptr;

    // Attempt to connect to an address until one succeeds
    for (ptr = pAddressInfo; ptr != NULL; ptr = ptr->ai_next) {
        // Create a SOCKET for connecting to server
        *ppOut = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (*ppOut == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            throw 1;
        }

        // Connect to server.
        iResult = connect(*ppOut, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(*ppOut);
            *ppOut = INVALID_SOCKET;
            continue;
        }
        break;
    }

    if (*ppOut == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        throw 1;
    }
}

void SendBufferMessage(std::string strSendMessage, SOCKET pConnectSocket) {
    int iResult;

    // Send an initial buffer
    iResult = send(pConnectSocket, strSendMessage.c_str(), MAXBYTE, 0);
    if (iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        throw 1;
    }

    /*
    printf("Bytes Sent: %ld\n", iResult);

    // shutdown the connection since no more data will be sent
    iResult = shutdown(pConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        throw 1;
    }

    // Receive until the peer closes the connection
    do {
        iResult = recv(pConnectSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0)
            printf("Bytes received: %d\n", iResult);
        else if (iResult == 0)
            printf("Connection closed\n");
        else
            printf("recv failed with error: %d\n", WSAGetLastError());

    } while (iResult > 0);*/
}

int main(int argc, char** argv) {
    ADDR_INFO* pAddrInfo = nullptr;

    int iResult = 0;

    LogIn();

    WSADATA wsaData;
    iResult = InitAddressInfo(argv[1], &wsaData, &pAddrInfo);
    if (iResult != 0) {
        return 1;
    }

    SOCKET pConnectSocket = INVALID_SOCKET;
    try {
        ConnectServer(pAddrInfo, &pConnectSocket);
    }
    catch (int iException) {
        WSACleanup();
        return 1;
    }

    freeaddrinfo(pAddrInfo);


    std::thread receiveThread = std::thread(ReceiveThread, pConnectSocket);
    receiveThread.detach();

    std::string input;
    while (true) {
        std::cin >> input;
        if (input.compare("1") == 0) {
            break;
        }

        SendBufferMessage(input, pConnectSocket);
    }
    /*
    std::string strSendMessage;
    while (true) {
        std::cout << "입력 > ";
        std::cin >> strSendMessage;

    }*/

    // cleanup
    closesocket(pConnectSocket);
    WSACleanup();

    return 0;
}

void ReceiveThread(SOCKET pConnectSocket) {
    WSANETWORKEVENTS serverEvent;
    HANDLE hEvent = WSACreateEvent();
    char strReceiveMessage[MAXBYTE];
    int iIndex = 0;

    WSAEventSelect(pConnectSocket, hEvent, FD_READ | FD_CLOSE);

    while (true) {
        iIndex = WSAWaitForMultipleEvents(1, &hEvent, false, INFINITE, false);
        if ((iIndex != WSA_WAIT_FAILED) && (iIndex != WSA_WAIT_TIMEOUT)) {
            WSAEnumNetworkEvents(pConnectSocket, hEvent, &serverEvent);

            if (serverEvent.lNetworkEvents == FD_READ) {
                int receiveLength = recv(pConnectSocket, strReceiveMessage, MAXBYTE, 0);
                if (receiveLength > 0) {
                    std::cout << strReceiveMessage << "\n";
                }
            }
            else if (serverEvent.lNetworkEvents == FD_CLOSE) {
                std::cout << "서버가 종료됨\n";
            }
        }
    }
}