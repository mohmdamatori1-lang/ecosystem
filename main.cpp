#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>

#include <cstring>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
    typedef int socklen_t;
    #define CLOSE_SOCKET closesocket
    #define SOCKET_T SOCKET
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #define CLOSE_SOCKET close
    #define SOCKET_T int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

#if defined(Q_OS_ANDROID)
#include <QJniObject>
#endif

using namespace std;

void announce_server() {
    SOCKET_T serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    int ttl = 1;
    setsockopt(serverSocket, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl));

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(53317);
    inet_pton(AF_INET, "224.0.0.167", &serverAddress.sin_addr);

    while(true) {
        sendto(serverSocket, "SERVER", 6, 0, (sockaddr*)&serverAddress, sizeof(serverAddress));
        this_thread::sleep_for(chrono::seconds(2));
    }
}

void start_receiver_loop() {
    SOCKET_T server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        CLOSE_SOCKET(server_fd);
        return;
    }
    listen(server_fd, 3);

    while(true) {
        socklen_t addrlen = sizeof(address);
        SOCKET_T client_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (client_socket == INVALID_SOCKET) continue;

        uint32_t filename_len = 0;
        if (recv(client_socket, (char*)&filename_len, sizeof(filename_len), 0) <= 0) {
            CLOSE_SOCKET(client_socket);
            continue;
        }

        string output_filepath(filename_len, '\0');
        if (recv(client_socket, &output_filepath[0], filename_len, 0) <= 0) {
            CLOSE_SOCKET(client_socket);
            continue;
        }

        int64_t file_size = 0;
        if (recv(client_socket, (char*)&file_size, sizeof(file_size), 0) <= 0) {
            CLOSE_SOCKET(client_socket);
            continue;
        }

        ofstream outfile(output_filepath, ios::binary);
        if (!outfile.is_open()) {
            CLOSE_SOCKET(client_socket);
            continue;
        }

        char buffer[8192];
        int64_t total_received = 0;
        while (total_received < file_size) {
            int64_t bytes_to_read = sizeof(buffer);
            if (file_size - total_received < bytes_to_read) {
                bytes_to_read = file_size - total_received;
            }
            int bytes_received = recv(client_socket, buffer, (int)bytes_to_read, 0);
            if (bytes_received <= 0) break;
            outfile.write(buffer, bytes_received);
            total_received += bytes_received;
        }
        CLOSE_SOCKET(client_socket);
    }
}

string findServer() {
    SOCKET_T serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    int yes = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));
    
#if defined(_WIN32)
    DWORD tv = 3000; 
    setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
    timeval tv{3, 0};
    setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(53317);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress));

    ip_mreq mreq{};
    inet_pton(AF_INET, "224.0.0.167", &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;
    setsockopt(serverSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));

    char buf[2048];
    sockaddr_in from{};
    socklen_t len = sizeof(from);
    int n = recvfrom(serverSocket, buf, sizeof(buf), 0, (sockaddr*)&from, &len);
    CLOSE_SOCKET(serverSocket);

    if (n <= 0) return "";
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
    return string(ip);
}

void send_file_backend(const string& filepath) {
    string ip = findServer();
    if (ip.empty()) return;

    SOCKET_T clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) return;

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = inet_addr(ip.c_str());

    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        CLOSE_SOCKET(clientSocket);
        return;
    }

    ifstream file(filepath, ios::binary | ios::ate);
    if (!file.is_open()){
        CLOSE_SOCKET(clientSocket);
        return;
    }
    streamsize file_size = file.tellg();
    file.seekg(0, ios::beg);

    string filename = std::filesystem::path(filepath).filename().string();
    uint32_t filename_len = (uint32_t)filename.size();
    send(clientSocket, (char*)&filename_len, sizeof(filename_len), 0);
    send(clientSocket, filename.c_str(), filename_len, 0);
    send(clientSocket, (char*)&file_size, sizeof(file_size), 0);

    char buffer[8192];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0 ){
        streamsize bytes_to_send = file.gcount();
        send(clientSocket, buffer, (int)bytes_to_send, 0);
    }

    CLOSE_SOCKET(clientSocket);
}

// =========================================================================
// Frontend App
// =========================================================================
class AppleEcosystemApp : public QMainWindow {
public:
    AppleEcosystemApp(QWidget *parent = nullptr) : QMainWindow(parent) {
        setupUI();
        initConnections();

        std::thread(announce_server).detach();
        std::thread(start_receiver_loop).detach();
    }

private:
    void handleScanClicked() {
        deviceList->clear();
        deviceList->addItem("Scanning network...");
        refreshButton->setEnabled(false);

        std::thread([this]() {
            string foundIp = findServer();

            QMetaObject::invokeMethod(this, [this, foundIp]() {
                deviceList->clear();
                if (!foundIp.empty()) {
                    deviceList->addItem(QString::fromStdString("Server found at: " + foundIp));
                } else {
                    deviceList->addItem("No servers found.");
                }
                refreshButton->setEnabled(true);
            }, Qt::QueuedConnection);
        }).detach();
    }

    void handleSelectFile() {
        QString filePath = QFileDialog::getOpenFileName(this, "Select File");
        if (!filePath.isEmpty()) {
            selectedFilePath = filePath;
            fileStatusLabel->setText("File: " + filePath);
            sendButton->setEnabled(true);
        }
    }

    void handleSendFile() {
        if (selectedFilePath.isEmpty()) return;

        string pathToSend = selectedFilePath.toStdString();
        fileStatusLabel->setText("Sending...");
        sendButton->setEnabled(false);

        std::thread([this, pathToSend]() {
            send_file_backend(pathToSend);

            QMetaObject::invokeMethod(this, [this]() {
                QMessageBox::information(this, "Done", "File sent successfully!");
                fileStatusLabel->setText("No file selected.");
                sendButton->setEnabled(false);
                selectedFilePath.clear();
            }, Qt::QueuedConnection);
        }).detach();
    }

    QTabWidget *tabWidget;
    QListWidget *deviceList;
    QPushButton *refreshButton;
    QPushButton *selectFileButton;
    QPushButton *sendButton;
    QLabel *fileStatusLabel;
    QString selectedFilePath;

    void setupUI() {
        setWindowTitle("Ecosystem App");
        resize(500, 350);

        tabWidget = new QTabWidget(this);

        QWidget *deviceTab = new QWidget();
        QVBoxLayout *deviceLayout = new QVBoxLayout(deviceTab);
        deviceLayout->setContentsMargins(15, 15, 15, 15);
        deviceLayout->setSpacing(10);

        deviceList = new QListWidget(deviceTab);
        refreshButton = new QPushButton("Scan", deviceTab);

        deviceLayout->addWidget(deviceList);
        deviceLayout->addWidget(refreshButton);

        QWidget *fileTab = new QWidget();
        QVBoxLayout *fileLayout = new QVBoxLayout(fileTab);
        fileLayout->setContentsMargins(15, 15, 15, 15);
        fileLayout->setSpacing(10);

        fileStatusLabel = new QLabel("No file selected.", fileTab);
        selectFileButton = new QPushButton("Browse...", fileTab);
        sendButton = new QPushButton("Send", fileTab);
        sendButton->setEnabled(false);

        fileLayout->addWidget(selectFileButton);
        fileLayout->addWidget(fileStatusLabel);
        fileLayout->addWidget(sendButton);
        fileLayout->addStretch();

        tabWidget->addTab(deviceTab, "Devices");
        tabWidget->addTab(fileTab, "Transfer");

        setCentralWidget(tabWidget);
    }

    void initConnections() {
        connect(refreshButton, &QPushButton::clicked, this, &AppleEcosystemApp::handleScanClicked);
        connect(selectFileButton, &QPushButton::clicked, this, &AppleEcosystemApp::handleSelectFile);
        connect(sendButton, &QPushButton::clicked, this, &AppleEcosystemApp::handleSendFile);
    }
};

int main(int argc, char *argv[]) {
#if defined(_WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

#if defined(Q_OS_ANDROID)
    try {
        QJniObject activity = QJniObject::callStaticObjectMethod("org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
        if (activity.isValid()) {
            QJniObject context = activity.callObjectMethod("getApplicationContext", "()Landroid/content/Context;");
            if (context.isValid()) {
                QJniObject wifiManager = context.callObjectMethod("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;", QJniObject::getStaticObjectMethod("android/content/Context", "WIFI_SERVICE", "Ljava/lang/String;").object());
                if (wifiManager.isValid()) {
                    QJniObject multicastLock = wifiManager.callObjectMethod("createMulticastLock", "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;", QJniObject::fromString("EcosystemMulticastLock").object());
                    if (multicastLock.isValid()) {
                        multicastLock.callMethod<void>("acquire");
                    }
                }
            }
        }
    } catch (...) {}
#endif

    QApplication app(argc, argv);
    AppleEcosystemApp window;
    window.show();
    int result = app.exec();

#if defined(_WIN32)
    WSACleanup();
#endif

    return result;
}