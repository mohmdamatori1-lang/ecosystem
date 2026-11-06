#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <thread>
#include <chrono>
#include <arpa/inet.h>
using namespace std;

// void recv_message(){
//     int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

//     int yes = 1;
//     setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

//     sockaddr_in serverAddress{};
//     serverAddress.sin_family = AF_INET;
//     serverAddress.sin_port = htons(8080);
//     serverAddress.sin_addr.s_addr = INADDR_ANY;

//     bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
//     listen(serverSocket, 5);

//     int clientSocket = accept(serverSocket, nullptr, nullptr);

//     char buffer[1024] = { 0 };
//     ssize_t n = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
//     if (n > 0) {
//         buffer[n] = '\0';
//         cerr << "Message from client: " << buffer << "\n";
//     }

//     close(clientSocket);
//     close(serverSocket);
// }




void announce(){
    int serverSocket = socket(AF_INET, SOCK_DGRAM,0);
    int ttl = 1;
    setsockopt(serverSocket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(53317);
    inet_pton(AF_INET, "224.0.0.167", &serverAddress.sin_addr);

    while(true) {
        sendto(serverSocket, "SERVER", 6, 0,
               (sockaddr*)&serverAddress, sizeof(serverAddress));
        this_thread::sleep_for(chrono::seconds(2));
    }
}

int main(){
    thread(announce).detach();
    while (true) sleep(1); 
}