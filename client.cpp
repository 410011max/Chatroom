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
mutex cout_mtx;
int client_socket;
char client_name[200];

void catch_ctrl_c(int signal);
void shared_print(string str, bool endLine);
void send_message(int client_socket);
void recv_message(int client_socket);
string find_sticker(char c);

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
    signal(SIGINT, catch_ctrl_c);

    // 輸入使用者名稱並傳送給server
    
    cout << "Enter your name\n";
    while (cin.getline(client_name, 200))
    {
        if (strlen(client_name) > 0)
        {
            send(client_socket, client_name, sizeof(client_name), 0);
            break;
        }
    }
    char server_message[200];
    recv(client_socket, server_message, sizeof(server_message), 0);
    // 新使用者須註冊、舊使用者須登入
    char password[200];
    if (strcmp(server_message, "sign up") == 0)
    {
        cout << "Create your password:" << endl;
        cin.getline(password, 200);
        while (strlen(password) < 3)
        {
            cout << "Your password is too short! (must be at least 3 words)" << endl;
            cin.getline(password, 200);
        }
        send(client_socket, password, sizeof(password), 0);
        cout << "Welcome to join OS_2022 Chatroom!" << endl;
    }
    else if (strcmp(server_message, "sign in") == 0)
    {
        while (1)
        {
            cout << "Enter your password:" << endl;
            cin.getline(password, 200);
            send(client_socket, password, sizeof(password), 0);
            recv(client_socket, server_message, sizeof(server_message), 0);
            if (strcmp(server_message, "right") == 0)
                break;
            else if (strcmp(server_message, "wrong") == 0)
            {
                cout << "Password is wrong!" << endl;
                continue;
            }
            else
            {
                cout << "Something error!" << endl;
                exit(-1);
            }
        }
    }
    else if (strcmp(server_message, "user is online") == 0)
    {
        cout << "User is already online." << endl;
        exit_flag = true;
        close(client_socket);
        exit(-1);
    }
    else
    {
        cout << "Something error!" << endl;
        exit(-1);
    }

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

// fix synchronization problem for cout
void shared_print(string str, bool endLine = true)
{
    lock_guard<mutex> guard(cout_mtx); // lock cout_mtx 直到 guard 結束
    cout << str;
    if (endLine)
        cout << endl;
}

// Send message to server
void send_message(int client_socket)
{
    while (1)
    {
        // if the server type"#exit" to server, return this thread
        if (exit_flag)
            return;

        // cout << "You: ";
        shared_print("You: ", false);
        char str[200];
        cin.getline(str, 200);

        // lock_guard<mutex> guard(cout_mtx);
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
        // cout << time_info;
        shared_print(time_info, false);
    }
}

// Receive message from server
void recv_message(int client_socket)
{
    while (1)
    {
        // if the client type"#exit" to server, return this thread
        if (exit_flag)
            return;

        char name[200], str[200];
        // lock_guard<mutex> guard(cout_mtx);
        if(recv(client_socket, name, sizeof(name), 0)<=0)return;
        if(recv(client_socket, str, sizeof(str), 0)<=0)return;

        for (int i = 0; i < 5; i++) // Erase text "You: " from terminal
            shared_print("\b", false);
        // cout << '\b';
        time_t now = time(0);
        char *time_info = ctime(&now);
        // detect special instruction from server
        if (strcmp(str, "\n\t//////server closed//////\n\t//////press enter to end//////") == 0 ||
            strcmp(str, "\n\t!!!You were kiked out!!!\n") == 0)
        {
            // cout << str << endl;
            shared_print(str);
            exit_flag = true;
            t_send.detach();
            close(client_socket);
            return;
        }
        else if (str[0] == '#' && '9' >= str[1] && str[1] >= '0')
        {
            string sticker = find_sticker(str[1]);
            if(strcmp(name,client_name)==0)
                shared_print(string("You: ")  + sticker);
            else
            shared_print(string(name) + ":     " + sticker);
        }
        else
        {
            if (strcmp(name, "#NULL") != 0)
                shared_print(string(name) + ": " + str + "\n" + time_info, false);
            // cout << name << ": " << str << endl << time_info;
            else
                shared_print(str);
            // cout << str << endl;
        }
        // cout << "You: ";
        shared_print("You: ", false);
        fflush(stdout);
    }
}

string find_sticker(char c)
{   
    string sticker;
    switch (c)
    {
    case '0':
        sticker = R"(               
                ......                  ............. 
            .....;;...                ................ 
         .......;;;;;/mmmmmmmmmmmmmm\/.................. 
       ........;;;mmmmmmmmmmmmmmmmmmm..................... 
     .........;;m/;;;;\mmmmmm/;;;;;\m...................... 
  ..........;;;m;;mmmm;;mmmm;;mmmmm;;m...................... 
..........;;;;;mmmnnnmmmmmmmmmmnnnmmmm\.................... 
.........  ;;;;;n/#####\nmmmmn/#####\nmm\................. 
.......     ;;;;n##...##nmmmmn##...##nmmmm\............. 
....        ;;;n#.....|nmmmmn#.....#nmmmmm,l......... 
 ..          mmmn\.../nmmmmmmn\.../nmmmm,m,lll..... 
          /mmmmmmmmmmmmmmmmmmmmmmmmmmm,mmmm,llll.. 
      /mmmmmmmmmmmmmmmmmmmmmmm\nmmmn/mmmmmmm,lll/ 
   /mmmmm/..........\mmmmmmmmmmnnmnnmmmmmmmmm,ll 
  mmmmmm|...........|mmmmmmmmmmmmmmmmmmmmmmmm,ll 
  \mmmmmmm\......./mmmmmmmmmmmmmmmmmmmmmmmmm,llo 
    \mmmmmmm\.../mmmmmmmmmmmmmmmmmmmmmmmmmm,lloo 
      \mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm,ll/oooo 
         \mmmmmmmmmmll..;;;.;;;;;;/mmm,lll/oooooo\ 
                   ll..;;;.;;;;;;/llllll/ooooooooo 
                   ll.;;;.;;;;;/.llll/oooooooooo..o 
                   ll;;;.;;;;;;..ll/ooooooooooo...oo 
                   \;;;;.;;;;;..ll/ooooo...ooooo..oo\ 
                 ;;;;;;;;;;;;..ll|oooo.....oooooooooo 
                ;;;;;;.;;;;;;.ll/oooo.....ooooooo....\ 
                ;;;;;.;;;;;;;ll/ooooooooooooo.....oooo 
                 \;;;.;;;;;;/oooooooooooo.....oooooooo\ 
                  \;;;.;;;;/ooooooooo.....ooooooooooooo 
                    \;;;;/ooooooo.....ooooooooooo...ooo\ 
                    |o\;/oooo.....ooooooooooooo......ooo 
                    oooooo....ooooooooooooooooooo.....oo\ 
                   oooo....oooooooooooooooooooooooo..oooo 
                  ___.oooooooooooooo....ooooooooooooooooo\ 
                 /XXX\oooooooooooo.....ooooooooooooooooooo 
                 |XXX|ooooo.oooooo....ooooooooooooooooooooo 
               /oo\X/oooo..ooooooooooooooooooo..oooooooooooo 
             /ooooooo..ooooo..oooooooooooooo.....ooooooooo... 
           /ooooo...ooooo.....oooooooooooo.......ooooooo.....o)"; // just a cute code
        break;
    case '1':
        sticker = R"(                                        
             ▄▄██████████▄▄             
             ▀▀▀   ██   ▀▀▀             
     ▄██▄   ▄▄████████████▄▄   ▄██▄     
   ▄███▀  ▄████▀▀▀    ▀▀▀████▄  ▀███▄   
  ████▄ ▄███▀              ▀███▄ ▄████  
 ███▀█████▀▄████▄      ▄████▄▀█████▀███ 
 ██▀  ███▀ ██████      ██████ ▀███  ▀██ 
  ▀  ▄██▀  ▀████▀  ▄▄  ▀████▀  ▀██▄  ▀  
     ███           ▀▀           ███     
     ██████████████████████████████     
 ▄█  ▀██  ███  ▀██    ███  ███  ██▀  █▄ 
 ███  ███ ███   ██    ███  ███▄███  ███ 
 ▀██▄████████   ██    ███  ████████▄██▀ 
  ▀███▀ ▀████   ██    ███  ████▀ ▀███▀  
   ▀███▄  ▀███████    ███████▀  ▄███▀   
     ▀███    ▀▀██████████▀▀▀   ███▀     
       ▀     ▄▄▄   ██   ▄▄▄     ▀       
             ▀▀██████████▀▀)";
        break;
    case '2':
        sticker = R"(
                 ."-,.__
                 `.     `.  ,
              .--'  .._,'"-' `.
             .    .'         `'
             `.   /          ,'
               `  '--.   ,-"'
                `"`   |  \
                   -. \, |
                    `--Y.'      ___.
                         \     L._, \
               _.,        `.   <  <\                _
             ,' '           `, `.   | \            ( `
          ../, `.            `  |    .\`.           \ \_
         ,' ,..  .           _.,'    ||\l            )  '".
        , ,'   \           ,'.-.`-._,'  |           .  _._`.
      ,' /      \ \        `' ' `--/   | \          / /   ..\
    .'  /        \ .         |\__ - _ ,'` `        / /     `.`.
    |  '          ..         `-...-"  |  `-'      / /        . `.
    | /           |L__           |    |          / /          `. `.
   , /            .   .          |    |         / /             ` `
  / /          ,. ,`._ `-_       |    |  _   ,-' /               ` \
 / .           \"`_/. `-_ \_,.  ,'    +-' `-'  _,        ..,-.    \`.
.  '         .-f    ,'   `    '.       \__.---'     _   .'   '     \ \
' /          `.'    l     .' /          \..      ,_|/   `.  ,'`     L`
|'      _.-""` `.    \ _,'  `            \ `.___`.'"`-.  , |   |    | \
||    ,'      `. `.   '       _,...._        `  |    `/ '  |   '     .|
||  ,'          `. ;.,.---' ,'       `.   `.. `-'  .-' /_ .'    ;_   ||
|| '              V      / /           `   | `   ,'   ,' '.    !  `. ||
||/            _,-------7 '              . |  `-'    l         /    `||
. |          ,' .-   ,' ||               | .-.        `.      .'     ||
 `'        ,'    `".'    |               |    `.        '. -.'       `'
          /      ,'      |               |,'    \-.._,.'/'
          .     /        .               .       \    .''
        .`.    |         `.             /         :_,'.'
          \ `...\   _     ,'-.        .'         /_.-'
           `-.__ `,  `'   .  _.>----''.  _  __  /
                .'        /"'          |  "'   '_
               /_|.-'\ ,".             '.'`__'-( \
                 / ,"'"\,'               `/  `-.|" mh)";
        break;
    case '3':
        sticker = R"(
                    >,o~o\/)-'     ,-'         \       ,.__.,-
                    <\__,'c~      (.---..____..,'      >'oo \~)
                       |--'_.---~~~~:     __.......__ `-`---'(
                      ',-~~         |   /'           `~~-. `'       .-~~~~. 
                     ','            ; ,'                  |    '~~\  |     `.
                   ,','            ; ;                    |  ,'    | |      |
                 ,.|,'            / |                     |  |     | `.     |
                | \`|           ,'  |                     | _.--,  `. |     `.
                |  | \      _.-'___                   _.--~~,-',    | |      |
                |  `. |--__..-~~   ~~~~~---...___.--~~ _,-'~ ,|     | `.__,--'
                |   | /';                     ./'   _,'     / |_,--~~
                `   /' |                _.---'  _,-'      ,'
                 \/'   |              /'      ~~ ~~--._   ;
       `\ `\    ,','~~\|            /'   __        .--'  /__,   __
         `\ `\_.||  ;  ||         /'  ,-~  `~.    '----~~~_.--~~
           `\/`  |  |  ;______  ,'   |  ,'    |       _.-~  ;               _,
      --..__|    `\_`,'\     _`\     `. |     ;   .-~~   __..----~~~~~      \
       _,-~ |  `-       `--~~          `----~'_..-    ~~~  /                |
     /'--.   \                           _       / ,'  ~~~7~~~~~~~~         |/
   ,'     ~-. `--.------------....__   /'_\_...-'       ,'`-.      /        |-
,-~~~~-._   /   (`-\___|___;...--'--~~~~~            ,-'     |    |         |~
/~~~-    ~~~     `\                            _,__,'   ___,-'    |         |
`-.                `-..___...__            _.-'/    ~~~~          ;         ;
`--`\____       __..---~~ ~~--..~--------~~   |                  ,'       ,')";
        break;
    case '4':
        sticker = R"(
         ⢀⣴⣤⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⠀⠀⠿⣿⣤⣾⣾⣿⣿⣾⣿⣿⣿⣧⡀⠀⠉⠁⠉⢿⡿⣧⣿⠀⢘⣿⡇⡟⠀⠘⣿⡇⠀⠸⢸⣿⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⢸⣿⣿⢿⣷⣦⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⠀⠀⣻⣿⡿⠋⠀⠀⡇⣼⣿⣿⣿⣿⣷⣄⠀⠀⠀⠈⣷⡘⣿⠀⢈⣿⠁⣷⠂⢰⣿⡇⠀⢰⢸⣏⣆⣆⠀
⠀⠀⠀⠀⠀⠀⠀⣼⠏⠈⠀⠹⣿⣿⣷⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣠⣾⡿⠋⠀⠀⠀⠀⡇⣿⣿⣿⣿⣿⣿⣿⣆⠀⠀⠀⠸⣇⣻⠀⣼⣿⡶⣿⠀⠸⣿⡇⠀⢸⣾⣿⠙⠉⠀
⠀⠀⠀⠀⠀⠀⠀⣿⠀⠀⠀⠀⠈⠻⣿⣿⣿⣦⡀⠀⠀⠀⠀⠀⢠⣿⡿⠋⣴⣾⠀⠀⠀⠀⣧⣿⣿⣿⣿⣿⣿⣿⣿⣆⠀⠀⠀⢻⣿⠀⣿⣿⣧⡟⠀⠀⡟⡇⠀⢸⣿⡟⣤⠁⠀
⠀⠀⠀⠀⠀⠀⠀⣿⡄⠀⠀⠀⠀⠀⠝⣿⣿⣿⣿⣦⡀⠀⠀⠀⡿⠟⠀⠀⡇⢸⠀⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡄⠀⠀⠘⣿⠀⠹⣿⠻⣷⡀⣁⡇⡇⠀⢸⢿⡇⢿⣀⠀
⠀⠀⠀⠀⠀⠀⠀⢹⡇⠀⠀⠀⠀⠀⠀⠘⣿⣿⣿⣿⣿⣦⡀⠀⡇⠀⠀⠀⡇⢸⠀⠀⠀⢀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⠀⠀⠀⣿⠀⠀⣿⠀⠹⣿⡍⡇⡇⠀⢸⢸⡇⡈⠉⠀
⠀⠀⠀⠀⠀⠀⠀⢸⡿⠀⠀⠀⠀⠀⠀⠀⢻⣿⣿⣿⣿⣿⣷⣤⡇⠀⠀⣨⣧⣾⣶⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣄⠀⠀⢻⡇⠀⣿⠀⠀⠘⣷⡇⡇⠀⢸⢸⡇⡇⠀⠀
⠀⠀⠀⠀⠀⠀⠀⢘⡗⠂⠀⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⣸⡇⠀⣿⠀⠀⠀⠘⢿⡇⠀⠸⢸⡏⠻⠀⠀
⠒⠀⠀⠀⠀⠀⠀⠀⣷⡀⠀⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⣠⣿⠀⠀⠀⠀⠈⡇⠀⢸⢸⡇⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⢸⣧⠀⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠟⠛⠻⢿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⡇⠀⢸⢸⡿⣶⢤⣴
⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⡄⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠋⠀⠀⠀⠀⠀⠙⣿⣿⣿⣿⡄⠀⠀⠀⠀⡇⠀⠀⣼⣷⡷⠟⠛
⣶⣦⣤⣤⣤⣤⣤⣤⣤⣾⣧⠀⠀⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠇⠀⠀⠀⠀⠀⠀⠀⠈⢻⣿⣿⣿⠀⠀⠀⠀⣧⠀⠾⣿⣿⠍⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣇⠀⠀⠠⢾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡀⠀⢸⣿⣆⠀⠀⠀⠀⣸⣿⣿⣿⣇⣤⠶⠋⡏⠀⢠⢸⠉⠁⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡄⠀⢀⣿⣿⣿⣿⣿⡿⠛⠁⠀⠀⠀⠈⠙⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠸⣿⣿⠄⠀⠀⢠⣿⣿⣿⣿⡏⠁⠀⠀⡇⠀⠀⢸⠀⠀⠸⡁
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡆⣸⣿⣿⣿⣿⡿⠁⠀⠀⠀⠀⠀⣀⠀⠈⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⣄⠈⠉⠀⢀⣴⣿⣿⣿⣿⣿⠇⠀⠀⠀⣇⠀⣠⣾⡀⠤⠄⠻
⣿⣿⣟⣿⣿⣿⣿⣿⣿⣿⠟⠁⠀⠻⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⣼⣿⣧⠀⠸⣿⣿⣿⣿⣿⣿⣫⣥⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡴⠖⠒⠚⡟⠀⠉⢹⠀⠀⠀⠀
⠀⠀⠈⠙⠿⢿⣭⣿⣿⠋⠀⠀⠀⠀⣿⣿⣿⣿⣿⣇⠀⠀⠀⠀⠀⠘⣿⡿⠀⢠⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠃⠀⠀⠀⠀⡇⠀⠀⢺⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠈⢿⣿⣷⡀⠀⠀⠀⠀⢹⣿⣿⣿⣿⣿⣧⡀⠀⠀⠀⠀⠀⠀⣠⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⡇⠀⠀⢸⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠛⠋⠁⠀⠀⠀⠀⠈⣿⣿⣿⣿⣿⣿⣿⣶⣤⣤⣤⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡟⢥⣿⠀⠀⠀⠀⠀⡇⠀⠰⢾⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⣠⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠟⣿⠀⣿⣿⠀⠀⠀⠀⠀⡇⠀⠀⢸⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠰⠶⠟⠋⠉⠉⠉⠀⠀⠀⠀⠈⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡆⣿⠀⢻⣿⠀⠀⠀⠀⠀⡇⠀⠀⢸⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠛⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⣿⠀⠀⠀⠀⠀⡅⠀⢨⢸⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠤⠀⠀⠀⠀⠀⠀⣀⡴⠛⠁⠀⠉⠛⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣇⢰⣿⠀⠀⠀⠀⠀⡇⠀⠘⢸⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⠞⠉⠀⠀⠀⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠂⠀⠀⢸⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣾⡋⠄⠀⠀⠀⠒⠲⢤⡀⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀⢀⣸⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⣟⠿⠋⠀⠀⠀⠀⠀⠀⠀⠈⣽⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⠀⠀⠀⠀⡅⠀⢸⣿⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠳⢄⠀⠀⠀⠀⠀⠠⠀⢀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡆⠀⠀⠀⠀⠀⠀⢸⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⠃⠀⠀⠀⠀⠀⠀⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡄⠀⠀⡄⠀⠈⢺⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⡇⠀⠈⣿⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⠀⣠⡇⠀⠀⣿⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⠾⠋⡇⠀⢰⣿⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⢴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⡇⠀⢸⣿⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠃⠀⠀⡇⠀⢸⡟)";
        break;
    case '5':
        sticker = R"(
    ⢠⣴⣦⣤⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣠⣤⣤⡀⠀⠀
⠀⠀⢰⣿⠿⠏⠉⠛⢿⣶⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣤⣶⠿⠛⠉⣹⣿⣿⡆⠀
⠀⠀⣴⣿⣧⠀⠀⠀⠀⠙⢿⣷⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣶⠟⠋⠀⠀⠀⠰⠋⣿⡿⣷⡄
⠀⠀⢹⣧⠀⠀⠀⠀⠀⠀⠀⠘⢿⣦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣾⠟⠁⠀⠀⠀⠀⠀⠀⠘⠁⢠⡿⠁
⠀⠀⠀⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠙⢿⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣾⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⠃⠀
⠀⠀⠀⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⣷⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⠀⠀
⠀⠀⠀⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠙⠛⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣸⡟⠀⠀
⠀⠀⢀⣿⠷⠀⠀⠀⠀⠀⠀⠀⣀⣀⣤⣀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⣀⣀⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣿⠃⠀⠀
⠀⢠⡾⠁⠀⢀⠀⠀⠀⣠⣞⠋⠁⠀⠀⠀⠈⠉⠢⡀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠖⠉⠁⠀⠀⠀⣈⣉⣳⣄⣀⡀⠀⠀⠀⠀⠀⠈⢿⡄⠀⠀
⠀⣿⠃⢀⣴⣁⣴⠾⠛⠉⠉⠁⠉⠉⠁⠀⠀⠀⠀⠈⠀⠀⠀⠀⠀⠀⠀⠀⠎⠀⠀⠀⠀⠀⠁⠀⠀⠀⠀⠉⠉⠛⠷⣦⣀⢢⡀⠈⣿⡀⠀
⠀⣿⣴⣿⡿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⢿⣿⣆⣿⡇⠀
⠀⢹⣿⠟⠀⠀⠀⠀⠀⠀⠀⠀⣀⡤⠔⠒⠋⠑⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠐⠒⠒⠢⠤⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠻⣿⡟⠀⠀
⢠⣿⡏⠀⠀⠀⠀⠀⠀⢀⣤⠞⠋⠀⠀⣀⣠⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⣀⣀⠀⠀⠈⠛⢦⣄⠀⠀⠀⠀⠀⠀⠀⠀⢹⣿⠀⠀
⣸⡟⠀⠀⠀⠀⠀⢠⣶⠟⠁⠀⠀⣰⣾⣟⠉⢹⣿⣦⠀⠀⠀⠀⠀⠀⠀⠀⣾⣿⡏⠉⣿⣷⣄⠀⠀⠀⠈⠳⣦⡀⠀⠀⠀⡀⠀⠈⣿⡇⠀
⣿⡇⠀⢀⠄⠀⣴⡟⠁⠀⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣄⣀⠀⠀⠀⠀⣀⣸⣿⣿⣿⣿⣿⣿⣿⡆⠀⠀⠀⠀⠈⠻⣆⠀⠀⢹⣦⡀⢹⡇⠀
⣿⡇⣠⡏⢀⣾⠏⠀⠀⠀⠀⠀⠀⢻⣿⣿⠟⠋⠉⠀⠀⠉⠙⠛⠛⠋⠉⠁⠀⠉⠙⠻⢿⣿⡟⠀⠀⠀⠀⠀⠀⠀⠙⢷⡄⠀⣿⣷⣼⡇⠀
⣿⣿⣿⣁⣾⠃⠀⠀⠀⠀⠀⠀⣠⡾⠋⠀⠀⠀⠀⠀⣀⣤⣴⣶⣶⣤⣄⡀⠀⠀⠀⠀⠀⠙⠷⣄⠀⠀⠀⠀⠀⠀⠀⠀⠻⣦⣿⣿⠿⠃⠀
⠈⠉⣿⣿⠃⠀⠀⠀⠀⠀⢀⠜⠁⠀⠀⠀⠀⠀⠀⠰⣿⣿⣿⣿⣿⣿⣿⡗⠀⠀⠀⠀⠀⠀⠀⠈⠳⣄⠀⠀⠀⠀⠀⠀⠀⠙⣿⡇⠀⠀⠀
⠀⠀⣿⠇⠀⠀⠀⠀⠀⠀⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠛⠿⠿⠿⠟⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⢦⠀⠀⠀⠀⠀⠀⠀⢸⣿⠀⠀⠀
⠀⠀⣿⠀⠀⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠀⠀⠀⠀⢀⣤⣤⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢆⠀⠀⠀⠀⢀⠀⢸⣿⠀⠀⠀
⠀⠀⣿⡀⢸⣷⠀⠀⠀⠀⠀⡀⠀⠀⠀⠀⠀⠈⠉⠒⠶⠞⠛⠉⠉⠛⠳⠶⠒⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠘⡀⠀⠀⠀⢸⣧⣸⡟⠀⠀⠀
⠀⠀⠸⣧⣸⣿⠀⠀⠀⠀⣸⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣆⠀⠀⠁⠀⠀⠀⣾⠿⠿⠃⠀⠀⠀
⠀⠀⠀⠙⠛⢿⣇⣰⠀⠀⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣿⡄⠀⠀⠀⣠⣾⠋⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠙⢿⡄⠘⣿⡆⠀⠀⣧⠀⠀⠀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⠀⠀⢀⡼⠀⠀⣼⣿⡇⢠⡿⠟⠋⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠈⢿⣤⣿⡿⣦⣀⣹⣷⡀⠀⢷⠀⠠⠶⠖⠚⠛⠒⠒⠂⢀⡾⠀⣠⠞⢁⣠⣾⠿⣿⣷⡟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠋⠀⠈⠙⢻⡏⠉⠀⠘⢧⠀⠀⠀⠀⠀⠀⠀⠀⡼⠁⠀⠁⠀⠀⣿⡇⠀⠈⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣾⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣾⡟⠀⠀⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⣷⡀⠀⣀⣤⣤⣤⣀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣾⡟⠀⠀⢠⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⣷⡿⠟⠋⠉⢉⣿⡇⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⡿⠁⠀⠀⢸⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢿⡄⠀⠀⠀⣈⣿⣧⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⠃⠀⠀⠀⠘⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡔⠀⠀⠀⠀⠀⠘⣿⠶⣶⣶⣾⠿⠋⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⢀⣼⡿⣿⠀⠀⠀⠀⠀⠙⣆⠀⠀⠀⢄⡀⠀⠀⠀⠀⠀⣠⠎⠀⠀⠀⠀⠀⠀⠀⣿⠀⠈⠻⣿⣄⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⣾⣿⠀⣿⠀⠀⠀⠀⠀⠀⠈⠙⠦⣄⡀⠹⣦⠀⠀⠀⣰⠋⠀⠀⠀⠀⠀⠀⠀⠀⣿⠀⠀⠀⢹⣿⡀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⣿⡟⠀⢹⡄⠀⠀⠀⠀⠀⠀⠀⠀⠸⡉⠀⠈⠇⠀⢠⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⠀⠀⢠⢈⣿⠇⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⣿⡇⢰⡜⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⣧⠀⠀⠀⠀⣼⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⡟⠀⡄⣼⣿⣿⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠿⠿⢿⣷⣿⡆⠀⠀⠀⠀⠀⠀⠀⣸⣿⣿⣿⣶⣶⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⣀⣼⣷⣿⠋⠁⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠹⣷⠀⠀⠀⠀⠀⠀⣴⡿⠋⠁⠀⠀⠀⢿⣇⠀⢠⠀⣠⠀⣀⢀⣾⡟⠛⠋⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⣶⠾⢶⣦⣼⣾⠏⠀⠀⠀⠀⠀⠀⠀⠻⣶⣾⣷⣿⣷⣿⣾⠏)";
        break;

    case '6':
        sticker = R"(
⡏⠙⠻⢦⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣤⠶⠟⠛⠿⠗⣦⣄⠀⠀⠀⠀
⡇⠀⠀⠀⠀⠙⠦⣄⠀⠀⠀⠀⠀⠀⠀⢀⣴⠋⠀⠀⠀⠀⠀⠀⠀⠉⢧⡀⠀⠀
⠙⢄⠀⠀⠀⠀⠀⠙⠲⢦⣀⠀⠀⠀⠀⣸⠋⠀⠀⠀⢠⣤⠀⠀⠀⠀⠘⢳⡀⠀
⠀⠈⠳⣀⠀⠀⠀⠀⠀⠀⠉⠻⢦⣤⣼⠴⠖⠒⣄⠀⠈⠉⠀⠀⠀⠀⠀⠈⣵⠀
⠀⠀⠀⠈⠳⣄⠀⠀⠀⠀⠀⠀⢀⣿⠋⠀⠀⠀⠈⢆⠀⠀⠀⠀⠀⠀⠀⠀⠀⢣
⠀⠀⠀⠀⠀⠈⠳⢦⡀⠀⠀⠀⣸⠁⠀⠀⠀⠀⣠⣼⣷⡤⢤⣄⠀⠀⠀⠀⠀⣻
⠀⠀⠀⠀⠀⠀⠀⠀⠙⢦⣤⣴⠋⠀⠀⠀⢀⣴⣿⣿⢿⠁⢘⣻⠀⠀⠀⠀⠀⣿
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠻⣔⣢⣲⣼⡿⣿⣿⠟⠁⠀⢸⠾⠀⠀⠀⠀⠀⢸
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠫⣿⠋⠀⠀⠀⣾⡇⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⠇⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣿⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣸⡏)";
        break;
    case '7':
        sticker = R"(
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣤⣶⣄⢀⣀⣀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣤⣤⣤⣤⣤⣤⣤⣤⣀⣀⠀⠀⠹⣿⣿⣿⣿⠟⠁⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⣠⣴⠾⠛⠉⠉⠉⠀⠀⠀⠀⠀⠉⠉⠉⠛⠷⣶⣼⣿⣉⣁⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⣠⡾⠛⠉⠛⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⡏⠉⠙⠛⢶⣄⠀⠀⠀⠀⠀
⠀⠀⠀⢠⣾⠟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠰⣿⣧⠀⠀⠀⠀
⠀⠀⢠⣿⠃⠀⠀⠀⠀⠀⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠀⠀⠀⠈⢿⡇⠀⠀⠀
⠀⠀⠹⣿⡎⠀⠀⠀⣠⡾⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠀⠀⠀⠀⠀⢻⡆⠀⠀⣆⣿⡇⠀⠀⠀
⠀⠀⠀⣿⡀⠀⡀⠀⣿⠁⠀⢠⣾⣿⡿⠆⠀⠀⠀⠀⠀⠀⢀⣾⣿⡿⠆⠀⠀⠀⢈⣿⠀⣄⣾⡿⠁⠀⠀⠀
⠀⠀⠀⠘⣿⣧⣇⢀⣇⠀⠀⠘⣿⣿⣷⡆⠀⠀⠀⠀⠀⠀⠈⢿⣿⣷⠖⠀⠀⠀⢸⣿⣷⠿⠋⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠉⠙⠻⣿⣆⡀⠀⠀⠉⠉⠀⠀⠀⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⣸⣿⠁⣀⣀⣀⠀⠀⠀⠀
⠀⢀⣠⡶⠾⠛⠓⠶⣿⡟⠀⠀⠀⠀⠀⠀⠠⣾⣿⣿⣦⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⣿⠟⠋⠉⠛⠻⣦⡀⠀
⢀⣾⠋⠀⠀⠀⠀⠀⠘⣿⣠⣄⡀⠀⠀⠀⠀⠛⠿⠟⠁⠀⠀⠀⠀⠀⡀⡀⠀⣴⣾⡏⠀⠀⠀⠀⠀⠈⣿⡄
⢸⣯⠀⢠⠀⠀⢀⣄⣠⣿⠿⢿⣷⣤⣦⣀⣤⣤⣤⣤⣀⣀⣀⣼⣆⣼⣷⣿⡾⠿⢿⣧⣀⣦⠀⠀⣤⠀⣸⡧
⠘⠿⣷⣾⣷⣤⠾⠿⠛⠁⠀⠀⠀⠁⠀⠉⠉⠀⠀⠀⠈⠉⠉⠉⠉⠉⠁⠀⠀⠀⠀⠉⠛⠻⠷⠾⠿⠟⠛⠁)";
        break;
    case '8':
        sticker = R"(
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⣿⣿⡷⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⣿⡿⠋⠈⠻⣮⣳⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⣴⣾⡿⠋⠀⠀⠀⠀⠙⣿⣿⣤⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣤⣶⣿⡿⠟⠛⠉⠀⠀⠀⠀⠀⠀⠀⠈⠛⠛⠿⠿⣿⣷⣶⣤⣄⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣴⣾⡿⠟⠋⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠛⠻⠿⣿⣶⣦⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⣀⣠⣤⣤⣀⡀⠀⠀⣀⣴⣿⡿⠛⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠛⠿⣿⣷⣦⣄⡀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣤⣄⠀⠀
⢀⣤⣾⡿⠟⠛⠛⢿⣿⣶⣾⣿⠟⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠛⠿⣿⣷⣦⣀⣀⣤⣶⣿⡿⠿⢿⣿⡀⠀
⣿⣿⠏⠀⢰⡆⠀⠀⠉⢿⣿⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⠻⢿⡿⠟⠋⠁⠀⠀⢸⣿⠇⠀
⣿⡟⠀⣀⠈⣀⡀⠒⠃⠀⠙⣿⡆⠀⠀⠀⠀⠀⠀⠀⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⠇⠀
⣿⡇⠀⠛⢠⡋⢙⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣾⣿⣿⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⠀⠀
⣿⣧⠀⠀⠀⠓⠛⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⠛⠋⠀⠀⢸⣧⣤⣤⣶⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢰⣿⡿⠀⠀
⣿⣿⣤⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠉⠉⠻⣷⣶⣶⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣿⣿⠁⠀⠀
⠈⠛⠻⠿⢿⣿⣷⣶⣦⣤⣄⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⣿⣷⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣾⣿⡏⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠉⠙⠛⠻⠿⢿⣿⣷⣶⣦⣤⣄⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⠿⠛⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⢿⣿⡄⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠙⠛⠻⠿⢿⣿⣷⣶⣦⣤⣄⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢿⣿⡄⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠉⠛⠛⠿⠿⣿⣷⣶⣶⣤⣤⣀⡀⠀⠀⠀⢀⣴⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢿⡿⣄
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠉⠛⠛⠿⠿⣿⣷⣶⡿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⣿⣹
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣀⠀⠀⠀⠀⠀⠀⢸⣧
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⣿⣆⠀⠀⠀⠀⠀⠀⢀⣀⣠⣤⣶⣾⣿⣿⣿⣿⣤⣄⣀⡀⠀⠀⠀⣿
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⢿⣻⣷⣶⣾⣿⣿⡿⢯⣛⣛⡋⠁⠀⠀⠉⠙⠛⠛⠿⣿⣿⡷⣶⣿)";
        break;
    case '9':
        sticker = R"(
⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⠀⠀⠀⠀⠀⣠⣴⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢟⣁⣤⣶⣾⣿⣿⣿⣿⣿⠟⠁⠀⠀⠀⠀⠀⣀⣤⣶⣶⣾⣿⣿⣿⣿⣷⣦⣀⠀⠹⡄
⠀⠀⠀⠀⠀⠀⢀⡤⠚⠁⠀⠀⢀⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠟⠁⠀⠀⠀⣠⣤⣾⣿⣿⣿⣿⡿⠋⠀⠈⠛⢿⣿⣿⣿⣷⣄⢡
⠀⠀⠀⠀⢀⠴⠋⠀⠀⠀⠀⣴⣿⣿⣿⣿⣿⡿⠟⠛⠛⠻⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠃⠀⣀⣤⣾⣿⣿⣿⣿⣿⣿⣿⠋⠀⠀⠀⠀⠀⠀⠈⠻⣿⣿⣿⡎
⠀⠀⠀⠀⠀⠀⠀⠀⣀⢠⣾⣿⣿⣿⣿⡟⣫⠗⢿⣽⣦⡀⠀⡿⠀⠀⠀⠀⠈⠉⣹⠟⠉⠙⠛⠛⠻⢿⣯⣥⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⣿⣿⣿
⠀⠤⠀⠀⠀⠀⠀⢠⣿⣿⣿⣿⠿⡿⢃⡴⠃⠀⠿⠿⠿⡇⣼⠁⠀⠀⠀⠀⣠⠞⠁⠀⠀⠀⠀⠀⠀⠈⠁⠈⣙⣛⢿⣿⣿⣿⣿⣿⣿⡿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢿⣿
⠀⠀⠀⠀⠀⠀⢠⣯⡿⠟⠉⠀⣰⣣⡾⠁⠀⠀⢀⣀⣰⣇⠏⠀⠀⠀⣠⠞⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⡠⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⢿
⠀⠀⠀⠀⠀⣠⠟⠋⠀⠀⠀⢀⣟⡟⠀⣠⠤⠚⠋⢠⡽⡿⠀⠀⢀⡜⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⣿⡿⠛⠛⠛⢿⣿⡻⣿⣿⣷⣟⢶⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘
⠀⠀⢀⡴⠞⠁⠀⠀⠀⠀⠀⢸⣼⢠⡞⠁⠀⠀⡼⠁⢸⠃⢀⣰⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢿⣿⣿⠃⠀⢀⡴⠒⢾⢷⡬⡟⢿⣿⣿⣦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⢀⣴⣥⡶⠂⠀⠀⠀⠀⠀⠀⢸⠉⠘⡇⠀⠀⠀⡇⢠⡏⡴⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣿⠀⠀⢸⠡⢶⣶⢸⡇⢱⠈⠙⢿⣿⣿⣄⡀⠀⠀⠀⠀⠀⠀⢀
⣿⣿⡿⠁⠀⢀⡀⠀⠀⠀⠀⢰⠀⠤⠼⠒⠛⠛⠃⣼⠞⠁⠀⠉⠉⠙⠛⠛⠒⠲⠤⣤⣀⡀⠀⠀⠀⠀⠀⠀⠙⣿⣄⠀⠘⢦⣬⠞⢸⡇⠸⡆⡂⠀⠙⣿⣿⣯⣄⠀⠀⠀⠰⠇⠀
⣿⡿⢀⣠⡖⠉⠀⠀⠀⠀⠀⢸⠀⠀⠀⠀⠀⠀⠐⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠙⠲⠤⣀⠀⠀⠀⠈⠿⢷⣦⣀⠀⠀⣾⠁⢀⣧⡇⠀⠀⠈⢻⣿⣿⣂⠀⠀⠀⠀⠀
⣿⠔⢉⡞⠀⠀⠀⠀⡖⠀⠀⠘⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠀⠀⠀⠀⠀⠀⠉⠙⢻⣾⡁⠀⣸⢿⣿⡀⢀⠂⠀⠻⣿⣯⢇⠀⠀⠀⠀
⠁⢀⡟⠀⠀⠀⠀⣸⠁⠀⢠⡞⣏⠉⠉⢻⡇⠀⠀⠀⠤⠤⠒⠒⠒⠒⠒⠒⠒⠒⠂⠤⠤⠤⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⠤⢄⣉⡛⠻⣖⣏⠉⠁⠁⠀⠀⢻⣿⡿⡄⠀⠀⠀
⢠⡟⠀⢀⡀⠀⠀⡟⠀⣰⠋⣷⠈⣄⢦⡿⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠹⣆⠈⣮⠇⢀⣠⠮⢄⣈⣿⡇⡄⠀⠀⠀
⠏⢀⣤⡿⠀⠀⣸⠁⣴⠃⠀⠘⣷⡀⣼⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣠⣤⠤⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢈⡷⣿⡟⡿⣷⣶⠟⢻⡇⠐⠀⠀
⣰⣿⡿⠀⠀⠀⣟⡼⠁⠀⠀⠀⡟⠳⡞⠀⠀⠀⠀⠀⢀⣠⠴⠖⠊⠉⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⢿⢱⡏⣿⣦⠿⠁⠀⢰⡷⠀⠀⠀
⣿⣿⠁⠀⠀⢠⡟⠀⠀⠀⠀⢰⡿⣿⠇⠀⠀⠀⠴⠊⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⣤⢤⣄⠀⠀⠀⠀⠀⠀⠀⣀⠀⠀⠀⠀⠀⣼⠈⢧⣹⡽⠋⠀⠀⠀⠈⠁⠀⠀⠀
⣿⡏⠀⣠⢤⠞⠀⠀⠀⠀⠀⢸⠀⢹⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⣴⣟⡉⠀⠀⠀⠀⠙⠢⣄⠀⠀⠀⠀⠈⠙⠦⠀⠀⠀⡇⠀⢀⠟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⡿⢀⣾⣣⠏⠀⠀⠀⠀⠀⠀⢸⠀⠘⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣶⣿⣿⣿⣿⣿⣷⣶⣄⡀⠀⠀⠈⠳⣄⠀⠀⠀⠠⠤⣀⡠⠼⣃⢀⠟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⣿⣿⠋⠀⠀⠀⠀⠀⠀⠀⢸⠀⠀⠹⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣾⠉⠛⢻⣿⣿⣿⣿⣿⣿⣿⣿⠗⢦⡀⠀⠈⠳⡄⠀⠀⠀⠀⠀⠀⣸⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣯⣽⠃⠀⠀⠀⠀⠀⠀⠀⠀⢸⡇⠀⠀⠸⡄⠀⠀⠀⠀⠀⠀⠀⠀⣟⠈⠻⣄⡀⠈⠻⣿⣿⣿⣿⣿⣧⣀⡀⠙⢦⡀⠀⣸⣦⠤⠀⢀⣠⡰⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⡿⣷⣶⣶⣄⡀⠀⠀⠀⠀⠈⡿⣆⠀⠀⠹⡄⠀⠀⠀⠀⠀⠀⠀⠸⣄⠀⠀⠙⢦⡀⠉⢿⣿⣿⣿⣿⣿⣷⣶⣶⡽⠞⠉⠀⠀⠀⣸⡽⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣦⣤⣤⡀⠀⣿⠈⠳⣄⠀⠹⡄⠀⠀⠀⠀⠀⠀⠀⠈⠳⣄⠀⠀⠙⢦⡀⢻⣿⣿⣿⣿⣿⡟⠋⠀⠀⠀⠀⢀⡼⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣜⣀⡀⠈⠳⣀⠙⣆⠀⠀⠀⠀⠀⠀⠀⠀⠈⠳⢄⡀⠀⠉⠻⢿⣿⣿⡟⠋⠀⠀⠀⠀⣠⠞⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣤⠙⠛⣞⣦⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠲⣄⠀⠈⣩⠏⠀⠀⠀⣀⠴⠊⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣤⣬⣝⣧⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⠻⠁⠀⢀⠤⠚⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⠴⠚⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠃⢰⣿⣷⣤⡀⠀⠀⠀⠀⣀⡴⠋⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠟⠣⣿⣿⣿⣿⣿⣿⠟⠛⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣟⠈⠀⣿⣿⣿⣧⣿⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡏⠀⢸⣿⣿⣏⣿⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠟⠀⢀⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠀⣢⣼⣿⡿⣽⣿⡟⢤⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⢰⣿⣿⣿⠃⣿⣿⡇⡀⠙⢦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠁⢹⣿⣿⣿⣿⣼⣿⣿⣧⠉⢆⠀⠙⢦⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠯⣄⣾⣿⣯⣽⣿⣿⣿⣿⣿⣷⡈⢢⡀⠀⠑⢄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡏⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⡀⠳⡄⠀⠀⠙⠦⣄)";
        break;
    }
    return sticker;
}
