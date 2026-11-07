#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <set>

using namespace std;


void send_message(const string& ip){
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = inet_addr(ip.c_str());
    connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

    string message_from_user;
    getline(cin, message_from_user);
    send(clientSocket, message_from_user.c_str(), message_from_user.size(), 0);
    close(clientSocket);
}

string findServer(){
    int serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    int yes = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    timeval tv{3, 0};
    if (setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("SO_RCVTIMEO");
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(53317);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("bind");
    }

    ip_mreq mreq{};
    inet_pton(AF_INET, "224.0.0.167", &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;
    if (setsockopt(serverSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("IP_ADD_MEMBERSHIP");
    }

    char buf[2048];
    sockaddr_in from{};
    socklen_t len = sizeof(from);
    ssize_t n = recvfrom(serverSocket, buf, sizeof(buf), 0, (sockaddr*)&from, &len);
    close(serverSocket);

    if (n <= 0) return "";
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
    return std::string(ip);
}

int main(){
    string ip = findServer();
    if (ip.empty()) {
        cerr << "No server found\n";
        return 1;
    }
    cerr << "Found server at " << ip << "\n";
    send_message(ip);
}