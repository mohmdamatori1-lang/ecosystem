#include <cstring>
#include <iostream>
#include <fstream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <string>
#include <filesystem>

using namespace std;

string findServer(){
    int serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    int yes = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    timeval tv{3, 0};
    setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(53317);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress));

    ip_mreq mreq{};
    inet_pton(AF_INET, "224.0.0.167", &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;
    setsockopt(serverSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    char buf[2048];
    sockaddr_in from{};
    socklen_t len = sizeof(from);
    ssize_t n = recvfrom(serverSocket, buf, sizeof(buf), 0, (sockaddr*)&from, &len);
    close(serverSocket);

    if (n <= 0) return "";
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
    return string(ip);
}

void send_file(const string& filepath){
    // files between 1mb to 200mb max
    string ip = findServer();
    if (ip.empty()) {
        cerr << "No server found via multicast.\n";
        return;
    }
    cerr << "Found server at " << ip << "\n";

    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Socket creation failed\n";
        return;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = inet_addr(ip.c_str());

    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        cerr << "Connection to server failed.\n";
        close(clientSocket);
        return;
    }

    ifstream file(filepath, ios::binary | ios::ate);
    if (!file.is_open()){
        cerr << "File not found: " << filepath << "\n";
        close(clientSocket);
        return;
    }
    streamsize file_size = file.tellg();
    file.seekg(0, ios::beg);

    string filename = std::filesystem::path(filepath).filename().string();
    uint32_t filename_len = filename.size();
    send(clientSocket, &filename_len, sizeof(filename_len), 0);
    send(clientSocket, filename.c_str(), filename_len, 0);

    send(clientSocket, &file_size, sizeof(file_size), 0);

    char buffer[8192];
    streamsize total_sent = 0;

    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0 ){
        streamsize bytes_to_send = file.gcount();
        ssize_t bytes_sent = send(clientSocket, buffer, bytes_to_send, 0);
        if (bytes_sent < 0) {
            cerr << "Error sending file chunk.\n";
            break;
        }
        total_sent += bytes_sent;        
        int progress = (total_sent * 100) / file_size;
        cout << "\rProgress: " << progress << "%" << flush;
    }

    cout << "\nFile transfer complete. Total sent: " << total_sent << " bytes\n";
    
    close(clientSocket);
}

void send_message(){


    string ip = findServer();

    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = inet_addr(ip.c_str());
    connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

    string message_from_user;
    cout << "message : " ;
    getline(cin, message_from_user);
    send(clientSocket, message_from_user.c_str(), message_from_user.size(), 0);
    close(clientSocket);
}



int main(){
    string filepath;
    cout << "file path: ";
    cin >> filepath ;

    send_file(filepath);
}