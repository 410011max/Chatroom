#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <ctime>
#include <errno.h>
#include <mutex>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
using namespace std;

bool exit_flag = false;
thread t_send, t_recv;
int client_socket;

void catch_ctrl_c(int signal);
// string color(int code);
void send_message(int client_socket);
void recv_message(int client_socket);

int main()
{
    // 創建socket
    // AF_INET 表示使用 IPv4 地址，SOCK_STREAM 表示使用面向連接的傳輸方式，IPPROTO_TCP 表示使用 TCP 協議
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    // 向服務器（指定的IP和port）發起請求
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));           // 初始化變數內容
    serv_addr.sin_family = AF_INET;                     // 使用IPv4
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //指定IP地址
    serv_addr.sin_port = htons(2022);                   //指定port

    // 透過server端listen()，與server端的accept()相接
    if (connect(client_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    { // 三次握手連接TCP
        cout << "Can't connect to Server.\n";
        exit(-1);
    }

    // 輸入使用者名稱並傳送給server
    char new_name[200];
    cout << "Enter your name\n";
    cin.getline(new_name, 200);
    send(client_socket, new_name, sizeof(new_name), 0);

    // 開啟兩個執行緒，平行處理接收與傳送資料
    thread t1(send_message, client_socket);
    thread t2(recv_message, client_socket);

    // unknown
    t_send = move(t1);
    t_recv = move(t2);

    //檢查可否join，並結束執行續
    if (t_send.joinable())
        t_send.join();
    if (t_recv.joinable())
        t_recv.join();

    // 關閉socket
    close(client_socket);

    return 0;
}

// Handler for "Ctrl + C"
void catch_ctrl_c(int signal)
{
    char str[200] = "#exit";
    send(client_socket, str, sizeof(str), 0);
    exit_flag = true;
    t_send.detach();
    t_recv.detach();
    close(client_socket);
    exit(signal);
}

// Send message to server
void send_message(int client_socket)
{
    while (1)
    {
        cout << "You: ";
        char str[200];
        cin.getline(str, 200);
        send(client_socket, str, sizeof(str), 0);
        time_t now = time(0);
        char *time_info = ctime(&now);
        if (strcmp(str, "#exit") == 0)
        {
            exit_flag = true;
            t_recv.detach();
            close(client_socket);
            return;
        }
        cout << time_info;
    }
}

// Receive message from server
void recv_message(int client_socket)
{
    while (1)
    {
        if (exit_flag)
            return;
        char name[200], str[200];

        int bytes_received = recv(client_socket, name, sizeof(name), 0);
        if (bytes_received <= 0)
            continue;

        recv(client_socket, str, sizeof(str), 0);

        for (int i = 0; i < 5; i++) // Erase text "Yout: " from terminal
            cout << '\b';
        time_t now = time(0);
        char *time_info = ctime(&now);
        if (strcmp(name, "#NULL") != 0)
            cout << name << ": " << str << endl << time_info;
        else
            cout << str << endl;

        cout << "You: ";
        fflush(stdout);
    }
}
