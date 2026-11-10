#include <cstring>
#include <iostream>
#include <fstream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <arpa/inet.h>
#include <filesystem>

using namespace std;

void announce(){
    int serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
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

void recv_file() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    
    // [EDITED]: Removed broken line: string& output_filepath = ;

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        cerr << "Bind failed\n";
        close(server_fd);
        return;
    }

    listen(server_fd, 3);
    cout << "Server listening for incoming file on port 8080...\n";

    int addrlen = sizeof(address);

    int client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
    if (client_socket < 0) {
        cerr << "Accept failed\n";
        close(server_fd);
        return;
    }

    // [EDITED]: Read filename length and filename string from socket
    uint32_t filename_len = 0;
    ssize_t len_bytes = read(client_socket, &filename_len, sizeof(filename_len));
    if (len_bytes <= 0) {
        close(client_socket);
        close(server_fd);
        return;
    }

    string output_filepath(filename_len, '\0');
    ssize_t name_bytes = read(client_socket, &output_filepath[0], filename_len);
    if (name_bytes <= 0) {
        close(client_socket);
        close(server_fd);
        return;
    }

    int64_t file_size = 0;
    ssize_t size_bytes = read(client_socket, &file_size, sizeof(file_size));
    if (size_bytes <= 0) {
        cerr << "Failed to receive file size.\n";
        close(client_socket);
        close(server_fd);
        return;
    }
    cout << "Expecting file of size: " << file_size << " bytes\n";

    ofstream outfile(output_filepath, ios::binary);
    if (!outfile.is_open()) {
        cerr << "Failed to create output file.\n";
        close(client_socket);
        close(server_fd);
        return;
    }

    char buffer[8192];
    int64_t total_received = 0;

    while (total_received < file_size) {
        int64_t bytes_to_read = sizeof(buffer);
        if (file_size - total_received < bytes_to_read) {
            bytes_to_read = file_size - total_received;
        }

        ssize_t bytes_received = read(client_socket, buffer, bytes_to_read);
        if (bytes_received <= 0) {
            cerr << "Connection closed or error during transfer.\n";
            break;
        }

        outfile.write(buffer, bytes_received);
        total_received += bytes_received;

        int progress = (total_received * 100) / file_size;
        cout << "\rReceiving Progress: " << progress << "%" << flush;
    }

    cout << "\nFile successfully received and saved to " << output_filepath << "\n";

    close(client_socket);
    close(server_fd);
}

void recv_message(){
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    int yes = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    listen(serverSocket, 5);

    int clientSocket = accept(serverSocket, nullptr, nullptr);

    char buffer[1024] = { 0 };
    ssize_t n = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        cerr << "Message from client: " << buffer << "\n";
    }

    close(clientSocket);
    close(serverSocket);
}

int main(){
    thread(announce).detach();
    recv_file();
}