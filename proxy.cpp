////
//// Created by Anonymous275 on 3/3/2020.
////

#define ENET_IMPLEMENTATION
#include "include/enet.h"
#include <WinSock2.h>
#include <iostream>
#include <chrono>
#include <WS2tcpip.h>
#include <cstdio>
#include <string>
#include <thread>
#include <queue>

#define DEFAULT_BUFLEN 64000
#define DEFAULT_PORT "4445"
typedef struct {
    ENetHost *host;
    ENetPeer *peer;
} Client;
std::chrono::time_point<std::chrono::steady_clock> PingStart,PingEnd;
extern std::vector<std::string> GlobalInfo;
std::queue<std::string> RUDPData;
std::queue<std::string> RUDPToSend;
bool Terminate = false;
int ping = 0;
void CoreNetworkThread();

void AutoPing(ENetPeer*peer){
    while(!Terminate){
        enet_peer_send(peer, 0, enet_packet_create("p", 2, ENET_PACKET_FLAG_RELIABLE));
        PingStart = std::chrono::high_resolution_clock::now();
        Sleep(1000);
    }
}
void NameRespond(ENetPeer*peer){
    std::string Packet = "NR" + GlobalInfo.at(0);
    enet_peer_send(peer, 0, enet_packet_create(Packet.c_str(), Packet.length()+1, ENET_PACKET_FLAG_RELIABLE));
}

void RUDPParser(const std::string& Data,ENetPeer*peer){
    char Code = Data.at(0),SubCode = 0;
    if(Data.length() > 1)SubCode = Data.at(1);
    switch (Code) {
        case 'p':
            PingEnd = std::chrono::high_resolution_clock::now();
            ping = std::chrono::duration_cast<std::chrono::milliseconds>(PingEnd-PingStart).count();
            return;
        case 'N':
            if(SubCode == 'R')NameRespond(peer);
            return;
    }
    std::cout << "Received: " << Data << std::endl;
    RUDPData.push(Data);
}


#pragma clang diagnostic pop
#pragma clang diagnostic pop
void HandleEvent(ENetEvent event,Client client){
    switch (event.type){
        case ENET_EVENT_TYPE_CONNECT:
            printf("Client Connected port : %u.\n",event.peer->address.port);
            //Name Of the Server
            event.peer->data = (void *)"Connected Server";
            break;
        case ENET_EVENT_TYPE_RECEIVE:
            RUDPParser((char*)event.packet->data,event.peer);
            enet_packet_destroy(event.packet);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            printf ("%s disconnected.\n", (char *)event.peer->data);
            // Reset the peer's client information.
            event.peer->data = nullptr;
            break;

        case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
            printf ("%s timeout.\n", (char *)event.peer->data);
            event.peer->data = nullptr;
            break;

        case ENET_EVENT_TYPE_NONE: break;
    }
}
void RUDPClientThread(const std::string& IP, int Port){
    if (enet_initialize() != 0) {
       std::cout << "An error occurred while initializing RUDP.\n";
    }

    auto start = std::chrono::high_resolution_clock::now();
    int Interval = 0;

    Client client;
    ENetAddress address = {0};

    address.host = ENET_HOST_ANY;
    address.port = Port;


    std::cout << "starting client...\n";

    enet_address_set_host(&address, IP.c_str());
    client.host = enet_host_create(nullptr, 1, 2, 0, 0);
    client.peer = enet_host_connect(client.host, &address, 2, 0);
    if (client.peer == nullptr) {
        std::cout << "could not connect\n";
    }
    std::thread Ping(AutoPing,client.peer);
    Ping.detach();
    do {
        ENetEvent event;
        enet_host_service(client.host, &event, 0);
        HandleEvent(event,client); //Handles the Events
        while (!RUDPToSend.empty()){
            ENetPacket* packet = enet_packet_create(RUDPToSend.front().c_str(),
                                                     RUDPToSend.front().length()+1,
                                                     ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(client.peer, 0, packet);
            std::cout << "sending : " << RUDPToSend.front() << std::endl;
            RUDPToSend.pop();
        }
        while(RUDPToSend.empty() && Interval < 1000){
            auto done = std::chrono::high_resolution_clock::now();
            Interval = std::chrono::duration_cast<std::chrono::milliseconds>(done-start).count();
        }
    } while (!Terminate);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
void TCPServerThread(const std::string& IP, int Port){
        Terminate = false;
        std::cout << "Proxy Started!" << std::endl;
        WSADATA wsaData;
        int iResult;
        SOCKET ListenSocket = INVALID_SOCKET;
        SOCKET ClientSocket = INVALID_SOCKET;

        struct addrinfo *result = nullptr;
        struct addrinfo hints{};

        int iSendResult;
        char recvbuf[DEFAULT_BUFLEN];
        int recvbuflen = DEFAULT_BUFLEN;

        // Initialize Winsock
        iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (iResult != 0) {
            std::cout <<"WSAStartup failed with error: " << iResult << std::endl;
        }

        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;

        // Resolve the server address and port
        iResult = getaddrinfo(nullptr, DEFAULT_PORT, &hints, &result);
        if ( iResult != 0 ) {
            std::cout << "getaddrinfo failed with error: " << iResult << std::endl;
            WSACleanup();
        }

        // Create a socket for connecting to server
        ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (ListenSocket == INVALID_SOCKET) {
            std::cout << "socket failed with error: " << WSAGetLastError() << std::endl;
            freeaddrinfo(result);
            WSACleanup();
        }

        // Setup the TCP listening socket
        iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            std::cout << "bind failed with error: " << WSAGetLastError() << std::endl;
            freeaddrinfo(result);
            closesocket(ListenSocket);
            WSACleanup();
        }

        freeaddrinfo(result);

        iResult = listen(ListenSocket, SOMAXCONN);
        if (iResult == SOCKET_ERROR) {
            std::cout << "listen failed with error: " << WSAGetLastError() << std::endl;
            closesocket(ListenSocket);
            WSACleanup();
        }
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            std::cout << "accept failed with error: " << WSAGetLastError() << std::endl;
            closesocket(ListenSocket);
            WSACleanup();
        }
        closesocket(ListenSocket);

        std::thread t1(RUDPClientThread,IP,Port);
        t1.detach();

        do {
            while(!RUDPData.empty()){
                iSendResult = send( ClientSocket, RUDPData.front().c_str(), int(RUDPData.front().length())+1, 0);
                if (iSendResult == SOCKET_ERROR) {
                    std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
                    closesocket(ClientSocket);
                    WSACleanup();
                }else{
                    RUDPData.pop();
                    std::cout << "Bytes sent: " << iSendResult << std::endl;
                }
            }

            iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
            if (iResult > 0) {
                std::string buff = recvbuf;
                buff.resize(iResult);
                RUDPToSend.push(buff);
                std::cout << "size : " << buff.size() << std::endl;
                std::cout << "Data : " << buff.c_str() << std::endl;
            }

            else if (iResult == 0) {
                std::cout << "Connection closing...\n";

            }
            else  {
                std::cout << "(Proxy) recv failed with error: " << WSAGetLastError() << std::endl;
                closesocket(ClientSocket);
                WSACleanup();
            }

        } while (iResult > 0);

        iResult = shutdown(ClientSocket, SD_SEND);
        if (iResult == SOCKET_ERROR) {
            std::cout << "shutdown failed with error: " << WSAGetLastError() << std::endl;
            closesocket(ClientSocket);
            WSACleanup();
        }

        closesocket(ClientSocket);
        WSACleanup();
        Terminate = true;
}



void ProxyStart(){
    std::thread t1(CoreNetworkThread);
    std::cout << "Core Network Started!\n";
    t1.join();
}

void ProxyThread(const std::string& IP, int Port){
    Terminate = false;
    std::thread t1(TCPServerThread,IP,Port);
    t1.detach();
}