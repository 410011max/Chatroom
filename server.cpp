#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
using namespace std;

struct client_information
{
    int id;
    string name;
    int socket;
    thread th;
    bool online;
};

// a global variable that records information of clients
vector<client_information> clients;
int counter = 0;
mutex cout_mtx, clients_mtx;
thread s_ctrl;

void set_name(int id, char name[]);
void shared_print(string str, bool endLine);
int broadcast_message(int num, int sender_id);
void end_connection(int id);
int find_user_id(string name);
void user_sign_in(int client_socket, int id);
void user_sign_up(int client_socket, int id);
void handle_client(int client_socket, int id);
// server control
void server_control(int server_socket);

int main()
{
    // 創建socket
    // AF_INET 表示使用 IPv4 地址，SOCK_STREAM 表示使用面向連接的傳輸方式，IPPROTO_TCP 表示使用 TCP 協議
    int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    //設定server socket 的IP及port
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));           // 初始化變數內容
    serv_addr.sin_family = AF_INET;                     // 使用IPv4
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //指定IP地址
    serv_addr.sin_port = htons(2022);                   // 指定port

    // 將server_socket綁定到指定IP及port，IP地址和端口都保存在 sockaddr_in structure中
    if (bind(server_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("bind error: "); // // 如果回傳-1表示有 bind error
        exit(-1);
    }

    cout << "Server is ready for client to connect!" << endl;
    // 透過 listen 監聽 client 發送的訊息
    if (listen(server_socket, 2) == -1)
    {                             // 收到connect()發来的消息才會繼續執行
        perror("listen error: "); // 如果回傳-1表示有 listen error
        exit(-1);
    }

    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);
    int client_socket;

    // create server control thread
    thread server_t(server_control, server_socket);
    s_ctrl = move(server_t);

    while (1)
    {
        // block 直到client connect成功
        if ((client_socket = accept(server_socket, (struct sockaddr *)&clnt_addr, &clnt_addr_size)) == -1)
        {
            perror("accept error: "); // 如果回傳-1表示有 accept error
            exit(-1);                 //
        }
        counter++; // 紀錄client編號

        thread t(handle_client, client_socket, counter); //為client 建立新的執行緒
        lock_guard<mutex> guard(clients_mtx);            // synchronisztion
        clients.push_back({counter, string("Anonymous"), client_socket, (move(t)), 0}); //將所有client資訊整合
    }

    //關閉socket
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].th.joinable())
            clients[i].th.join();
    }

    // join server control thread
    if (s_ctrl.joinable())
        s_ctrl.join();

    close(client_socket);
    close(server_socket);

    return 0;
}

// Set name of client
void set_name(int id, char name[])
{
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].id == id)
        {
            clients[i].name = string(name);
            clients[i].online = 1;
        }
    }
}
// For synchronisation of cout statements
void shared_print(string str, bool endLine = true)
{
    lock_guard<mutex> guard(cout_mtx); // lock cout_mtx 直到 guard 結束
    cout << str;
    if (endLine)
        cout << endl;
}

// Broadcast message to other clients
int broadcast_message(string message, int sender_id)
{
    char temp[200];
    strcpy(temp, message.c_str());
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].id != sender_id && clients[i].online == 1) // 不發送給自己、不發送給正在登入的
        {
            send(clients[i].socket, temp, sizeof(temp), 0);
        }
    }
}

// Clint end connection
void end_connection(int id)
{
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].id == id)
        {
            lock_guard<mutex> guard(clients_mtx); // lock 直到清除 client 資料結束
            clients[i].th.detach();               // 關閉對應thread
            close(clients[i].socket);             // Close the client socket
            clients.erase(clients.begin() + i);   // Erase client information
            break;
        }
    }
}

string find_user_password(string find_name)
{
    ifstream ifs;
    ifs.open("./user_list.csv");
    string name, password = "";

    while (ifs >> name >> password)
    {
        if (name == find_name)
            break;
        password = "";
    }

    ifs.close();

    return password;
}

void user_sign_up(int client_socket, string name)
{
    char message[200] = "sign up";
    send(client_socket, message, sizeof(message), 0); // 告訴 client 需要註冊
    char password[200];
    recv(client_socket, password, sizeof(password), 0); // 接收 client 註冊的密碼
    ofstream ofs;
    ofs.open("./user_list.csv", ios::app); // 儲存到資料庫
    ofs << name << "\n" << password << endl;
    ofs.close();
}

void user_sign_in(int client_socket, string correct_password_str)
{
    char message[200] = "sign in";
    send(client_socket, message, sizeof(message), 0); // 告訴 client 需要登入
    char password[200];
    char correct_password[200];
    strcpy(correct_password, correct_password_str.c_str());
    while (1)
    {
        recv(client_socket, password, sizeof(password), 0); // 接收 client 輸入的密碼
        if (strcmp(password, correct_password) == 0)        // 密碼正確
        {
            char message[200] = "right";
            send(client_socket, message, sizeof(message), 0);
            return;
        }
        char message[200] = "wrong"; // 密碼錯誤，需重新輸入
        send(client_socket, message, sizeof(message), 0);
    }
}

void handle_client(int client_socket, int id)
{
    char name[200], str[200];
    recv(client_socket, name, sizeof(name), 0);
    string password = find_user_password(name);

    if (password == "")
    { // sign up
        user_sign_up(client_socket, name);
        cout << name << "sign up successfully!" << endl;
    }
    else
    { // sign in
        user_sign_in(client_socket, password);
        cout << name << "sign in successfully!" << endl;
    }

    set_name(id, name); // 設定名稱及上線
    // Display welcome message
    string welcome_message = string("Welcome ") + string(name) + string(" to join OS_2022 Chatroom~");
    string name_list = "Online users:";
    for (int i = 0; i < clients.size(); i++)
    {
        name_list += " " + clients[i].name;
    }

    broadcast_message("#NULL", id);         // server 發送之公告，沒有發送者 (client) 名字
    broadcast_message(welcome_message, id); // 輸出 welcome 訊息到 client
    broadcast_message("#NULL", id);
    broadcast_message(name_list, id); // 輸出線上用戶名單到 client
    shared_print(welcome_message);    // 輸出 welcome 訊息到 server 畫面
    shared_print(name_list);          // 輸出線上用戶名單到 server 畫面

    while (1)
    {
        int bytes_received = recv(client_socket, str, sizeof(str), 0);
        if (bytes_received <= 0) // error(-1)或斷開連結(0)
            return;
        if (strcmp(str, "#exit") == 0)
        {
            end_connection(id);
            // Display leaving message
            string message = string(name) + string(" has left");
            string name_list = "Online users:";
            for (int i = 0; i < clients.size(); i++)
            {
                name_list += " " + clients[i].name;
            }
            broadcast_message("#NULL", id);
            broadcast_message(message, id); // 輸出離開訊息到 client
            broadcast_message("#NULL", id);
            broadcast_message(name_list, id); // 輸出線上用戶名單到 client
            shared_print(message);
            shared_print(name_list);
            return;
        }
        broadcast_message(string(name), id);
        broadcast_message(string(str), id);
        shared_print(string(name) + ": " + str);
    }
}

void server_control(int server_socket)
{
    char str[200];
    while (1)
    {
        cin.getline(str, 200);
        if (strcmp(str, "#exit") == 0)
        {
            cout << "Close server and clients\n";

            // close clients
            string message = string("\n\t//////server closed//////\n\t//////press enter to end//////");
            broadcast_message("#NULL", -1);
            broadcast_message(message, -1);
            // shared_print(message);
            cout << "\n\t//////server closed//////\n";

            while (clients.size() > 0)
            {
                lock_guard<mutex> guard(clients_mtx); // lock 直到清除 client 資料結束
                clients[0].th.detach();               // 關閉對應thread
                if (clients[0].th.joinable())
                    clients[0].th.join();
                close(clients[0].socket);       // Close the client socket
                clients.erase(clients.begin()); // Erase client information
            }

            // close server
            s_ctrl.detach();
            if (s_ctrl.joinable())
                s_ctrl.join();
            close(server_socket);

            // cout << "done!!\n";
            exit(0);
        }
        else if (strcmp(str, "#remove") == 0)
        {
            char victim[200];
            cout << "Enter victim's name:";
            cin.getline(victim, 200);
            cout << "Removing " << victim << "......" << endl;

            for (auto it = clients.begin(); it != clients.end(); it++)
            {
                cout << (*it).name << "(" << (*it).id << ")\n";
            }

            int id = -1, i = 0;
            for (i = 0; i < clients.size(); i++)
            {
                const char *name_char = clients[i].name.c_str();
                if (strcmp(name_char, victim) == 0)
                {
                    cout << "Find " << clients[i].name.c_str() << " with id = " << clients[i].id << endl;
                    const char *message = "#remove";
                    send(clients[i].socket, message, sizeof(message), 0);
                    lock_guard<mutex> guard(clients_mtx); // lock 直到清除 client 資料結束
                    clients[i].th.detach();               // 關閉對應thread
                    close(clients[i].socket);             // Close the client socket
                    clients.erase(clients.begin() + i);   // Erase client information
                    break;
                }
            }
            if (i == clients.size())
            {
                cout << victim << " not exist.\n";
            }
            else
            {
                string message = string(victim) + string(" has been removed.");
                broadcast_message("#NULL", -1);
                broadcast_message(message, -1);
                shared_print(message);
            }
            cout << "done\n";
        }
        else
        {
            cout << "nothing happened\n";
        }
    }
}
