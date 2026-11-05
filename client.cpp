#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

int main(){
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
//  this for testing
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    // serverAddress.sin_addr.s_addr = inet_addr("192.168.1.106");

    connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    string message_from_user;
    getline(cin, message_from_user);
    send(clientSocket, message_from_user.c_str(), message_from_user.size(), 0);
    close(clientSocket);
}


