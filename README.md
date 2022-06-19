# Chatroom
NCKU EE_OS_2022 Project 2  Many-to-Many Chatroom
## Step to run the Chatroom
1. Clone this repository
```
git clone https://github.com/410011max/Chatroom.git
```
2. Run the following commands in your terminal :
```
make
```
or
```
g++ server.cpp -lpthread -o server
g++ client.cpp -lpthread -o client
```
3. To run the server application, use this command in the terminal :
```
./server
```
4. Now, open another terminal and use this command to run the client application :
```
./client
```
5. For opening multiple client applications, repeat step 4 in other terminal.

## Step to use the Chatroom  
1. Sign in or Sign up: enter the user name and the password (if not sign up yet, it will automatically sign up).
2. After entering the chatroom, you(client) can send message to the chatroom, and everyone in the room can see the message.
3. Special manipulation/message.
  - on client terminal:
    - Summon cute sticker to your friends: enter "#" +"0"~"9."    
    - Randomly picking one user: enter "#lotto."
    - Exit the chatroom: enter "#exit."
  - on server terminal:
    - Terminate all client and server threads: enter "#exit."   
    - Remove a user: enter "#remove", and then enter the target's name. 
    - Clear all user data: enter "#clear."
