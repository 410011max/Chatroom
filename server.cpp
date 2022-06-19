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

struct client_information // 使用者資料
{
    int id; // 第幾個進入伺服器
    string name; // 名字
    int socket; // socket編號
    thread th; // 對應thread (handle_client)
    bool online; // 是否已經登入成功
};

// a global variable that records information of clients
vector<client_information> clients;
int counter = 0;
mutex cout_mtx, clients_mtx;
thread s_ctrl;

void set_name(int id, char name[]); // 設定id及名字
void shared_print(string str, bool newline); // thread 之間 cout 分配問題
void broadcast_message(string message, int sender_id);  // 傳送訊息給各個client
void end_connection(int id); // 結束client的連線
int find_user_id(string name); // 看資料庫是否有儲存使用者資料
bool user_sign_in(int client_socket, string name); // 登入
bool user_sign_up(int client_socket, string name); // 註冊
void user_is_online(int client_socket); // 判斷使用者是否在線
void handle_client(int client_socket, int id); // subthread 接取處理特定 client 訊息
void server_control(int server_socket); // 處理 server 功能

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

        thread t(handle_client, client_socket, counter); // 為client 建立新的執行緒
        lock_guard<mutex> guard(clients_mtx);            // synchronisztion
        clients.push_back({counter, string("Anonymous"), client_socket, (move(t)), 0}); //將所有client資訊整合
    }

    //關閉socket
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].th.joinable())
            clients[i].th.join();
    }

    // 檢查可否 join，並結束執行續
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
void shared_print(string str, bool newline = true)
{
    lock_guard<mutex> guard(cout_mtx); // lock cout_mtx 直到 guard 結束
    cout << str;
    if (newline) // 換行
        cout << endl;
}

// Broadcast message to other clients
void broadcast_message(string message, int sender_id)
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

// find password
string find_user_password(string find_name)
{
    for (int i = 0; i < clients.size(); i++) // 判斷 user 是否在線
    {
        if (clients[i].name == find_name)
        {
            return "user is online"; 
        }
    }
    ifstream ifs;
    ifs.open("./user_list.csv"); // 從資料庫找user資料
    string name, password = "";

    while (ifs >> name >> password)
    {
        if (name == find_name)
            break;
        password = "";
    }

    ifs.close();

    return password; // 回傳密碼，若找不到則回傳空字串""
}

// user sign up
bool user_sign_up(int client_socket, string name)
{
    char message[200] = "sign up";
    send(client_socket, message, sizeof(message), 0); // 告訴 client 需要註冊
    char password[200];
    if (recv(client_socket, password, sizeof(password), 0) <= 0)
    { // 接收 client 註冊的密碼
        cout << "oh no! sign up error!" << endl;
        return 0;
    }
    ofstream ofs;
    ofs.open("./user_list.csv", ios::app); // 儲存到資料庫
    ofs << name << "\n" << password << endl;
    ofs.close();

    return 1;
}

// user sign in
bool user_sign_in(int client_socket, string correct_password_str)
{
    char message[200] = "sign in";
    send(client_socket, message, sizeof(message), 0); // 告訴 client 需要登入
    char password[200];
    char correct_password[200];
    strcpy(correct_password, correct_password_str.c_str());
    while (1)
    {
        if (recv(client_socket, password, sizeof(password), 0) <= 0)
        { // 接收 client 輸入的密碼
            cout << "oh no! sign in error!" << endl;
            return 0;
        }
        if (strcmp(password, correct_password) == 0) // 密碼正確
        {
            char message[200] = "right";
            send(client_socket, message, sizeof(message), 0);
            return 1;
        }
        char message[200] = "wrong"; // 密碼錯誤，需重新輸入
        send(client_socket, message, sizeof(message), 0);
    }
}

// tell client not to repeat sign in
void user_is_online(int client_socket)
{
    char message[200] = "user is online";
    send(client_socket, message, sizeof(message), 0); // 告訴 client 不要重複登入
}

// handle client
void handle_client(int client_socket, int id)
{
    char name[200], str[200];
    if (recv(client_socket, name, sizeof(name), 0) <= 0) // 連結錯誤或已斷開
    {
        end_connection(id); // 關閉此client對應之thread及socket
        return;
    }
    string password = find_user_password(name);

    if (password == "user is online") // user已經在線
    {
        user_is_online(client_socket);
        end_connection(id);
        return;
    }
    else if (password == "") // 找不到user，須註冊
    { // sign up
        if (user_sign_up(client_socket, name) == 0) // 註冊error
        {
            end_connection(id);
            return;
        }
        cout << name << " sign up successfully!" << endl;
    }
    else // 找到user資料，須登入
    { // sign in
        if (user_sign_in(client_socket, password) == 0) // 登入erorr
        {
            end_connection(id);
            return;
        }
        cout << name << " sign in successfully!" << endl;
    }

    set_name(id, name); // 設定id、名稱、上線狀態
    // Display welcome message
    string welcome_message = string("Welcome ") + string(name) + string(" to join OS_2022 Chatroom~");
    string name_list = "Online users:";
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].online == 1)
            name_list += " " + clients[i].name;
    }

    broadcast_message("#NULL", id);         // server 發送之公告，沒有發送者 (client) 名字
    broadcast_message(welcome_message, id); // 輸出 welcome 訊息到 client
    broadcast_message("#NULL", id);
    broadcast_message(name_list + string("\n"), id); // 輸出線上用戶名單到 client
    shared_print(welcome_message);                   // 輸出 welcome 訊息到 server 畫面
    shared_print(name_list);                         // 輸出線上用戶名單到 server 畫面

    cout << R"(  
                                                __----~~~~~~~~~~~------___
                                   .  .   ~~//====......          __--~ ~~
                   -.            \_|//     |||\\  ~~~~~~::::... /~
                ___-==_       _-~~  \/    |||  \\            _/~~-
        __---~~~.==~||\=_    -_--~/_-~|-   |\\   \\        _/~
    _-~~     .=~    |  \\-_    '-~7  /-   /  ||    \      /
  .~       .~       |   \\ -_    /  /-   /   ||      \   /
 /  ____  /         |     \\ ~-_/  /|- _/   .||       \ /
 |~~    ~~|--~~~~--_ \     ~==-/   | \~--===~~        .\
          '         ~-|      /|    |-~\~~       __--~~
                      |-~~-_/ |    |   ~\_   _-~            /\
                           /  \     \__   \/~                \__
                       _--~ _/ | .-~~____--~-/                  ~~==.
                      ((->/~   '.|||' -_|    ~~-/ ,              . _||
                                 -_     ~\      ~~---l__i__i__i--~~_/
                                 _-~-__   ~)  \--______________--~~
                               //.-~~~-~_--~- |-------~~~~~~~~
                                      //.-~~~--\

                            神獸保佑    程式碼無BUG!
    )"
         << "\n";

    while (1)
    {
        if (recv(client_socket, str, sizeof(str), 0) <= 0) // error(-1)或斷開連結(0)
        {
            end_connection(id);
            string message = string(name) + string(" has left OS_2022 Chatroom QAQ");
            string name_list = "Online users:";
            for (int i = 0; i < clients.size(); i++)
            {
                if (clients[i].online == 1)
                    name_list += " " + clients[i].name;
            }
            broadcast_message("#NULL", id);
            broadcast_message(message, id); // 輸出離開訊息到 client
            broadcast_message("#NULL", id);
            broadcast_message(name_list + string("\n"), id); // 輸出線上用戶名單到 client
            shared_print(message);
            shared_print(name_list);
            return;
        }

        if (strcmp(str, "#exit") == 0) // client 要離開聊天室
        {
            printf("here!\n");
            end_connection(id);
            // Display leaving message
            string message = string(name) + string(" has left OS_2022 Chatroom QAQ");
            string name_list = "Online users:";
            for (int i = 0; i < clients.size(); i++)
            {
                if (clients[i].online == 1)
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
        else if (strcmp(str, "#lotto") == 0) // lotto功能
        {
            broadcast_message(string(name), id);
            broadcast_message(string(str), id);
            shared_print(string(name) + ": " + str);

            srand(time(NULL));               // 設定亂數種子
            int x = rand() % clients.size(); // 產生亂數
            string message = "\n\t----Good Luck to " + clients[x].name + ".----\n";
            broadcast_message("#NULL", -1);
            broadcast_message(message, -1); // 輸出離開訊息到 client
            shared_print(message);
            continue;
        }
        else if (str[0] == '#' && '9' >= str[1] && str[1] >= '0') // 貼圖功能
        {
            broadcast_message(string(name), id);
            broadcast_message(string(str), id);
            shared_print(string(name) + ": " + str);
            continue;
        }
        broadcast_message(string(name), id);
        broadcast_message(string(str), id);
        shared_print(string(name) + ": " + str);
    }
}

// server function
void server_control(int server_socket)
{
    char str[200];
    while (1)
    {
        cin.getline(str, 200);

        if (strcmp(str, "#exit") == 0 || strcmp(str, "#clear") == 0) //關閉伺服器 (清除or不清除資料)
        {
            if (strcmp(str, "#clear") == 0) // 清除資料
            {
                shared_print(string("Clear user list"));
                string file = "user_list.csv";
                remove(file.c_str());
            }

            shared_print(string("Close server and clients"));

            // close clients
            string message = string("\n\t//////server closed//////\n\t//////press enter to end//////");
            broadcast_message("#NULL", -1);
            broadcast_message(message, -1);
            shared_print(string("\n\t//////server closed//////\n"));

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
        else if (strcmp(str, "#remove") == 0) // 移除特定用戶
        {
            char victim[200];
            shared_print(string("Enter victim's name:"));

            cin.getline(victim, 200);
            shared_print(string("Removing ") + string(victim) + string("......"));

            string message = string("\n\t----") + string(victim) + string(" has been removed----\n");

            // for (auto it = clients.begin(); it != clients.end(); it++)
            // {
            //     cout << (*it).name << "(" << (*it).id << ")\n";
            // }
            bool exist = false;
            int i = 0;
            for (i = 0; i < clients.size(); i++)
            {
                const char *name_char = clients[i].name.c_str();
                if (strcmp(name_char, victim) == 0) // 找要remove的client
                {
                    exist = true;
                    // cout << "Find " << clients[i].name.c_str() << " with id = " << clients[i].id << endl;
                    break;
                }
            }
            if (!exist)
            {
                shared_print(string(victim) + string(" not exist."));
            }
            else
            {
                broadcast_message("#NULL", -1);
                broadcast_message(message, clients[i].id); //不傳給被踢的人

                string kik_message = "\n\t!!!You were kiked out!!!\n";

                char temp[200];
                strcpy(temp, kik_message.c_str());
                send(clients[i].socket, temp, sizeof(temp), 0); // 只傳給被踢的人

                shared_print(message);

                // 從server 端剔除
                lock_guard<mutex> guard(clients_mtx); // lock 直到清除 client 資料結束
                clients[i].th.detach();               // 關閉對應thread
                close(clients[i].socket);             // Close the client socket
                clients.erase(clients.begin() + i);   // Erase client information
            }
            // cout << "done\n";
        }
        else
        {
            shared_print(string("nothing happened")); // 找不到要remove的用戶
        }
    }
}