## Project Architecture
### Server
- Every time server starts, it creates a socket and use multiplexed I/O for two fds: one for accepting connection and one for reading from screen
- On reading from screen, it has following commands:
  - ```list conn```
  - ```list process```
  - ```exit```
- On accepting a connection from a client, it does two things:
  - Registers the connectionin ConnList
  - Creates two pipes and Fork the process for the client
    - The client process on server now implements multiplexed I/O for reading from client socket fd and reading end of the pipe
    - The client loops indefinitely anddoes either of the following things or both:
        - receiving commands from client end and processing them, and then writing on the socket
        - receivingcommand from pipe(server process)for sending its process list
 
#### Signals Used on Server Side
- ```SIGCHLD``` for client process on server: It is used when any of the processes, which are started from a client process, exit. It updates processList for the client process.
- ```SIGCHLD``` for server process:  It is used when any of the active connections disconnect. It updates the connList in the server process. 

Both the two signals reap as many zombies as they can using the combination of while loop and ```WNOHANG``` flag. It avoids the limitation of receiving only one signal when more than one signal of the same type arrived.


### Client
- Every time client starts, it prompts the user for connecting to a server with the following command:
```connect <ip> <port>```
- After connecting to server, three threads has been created.
  - **Thread1**: loops indefinitely for reading from screen and writing to server, it is detached thread. It is cancelled by thread 2
  - **Thread2**: loops indefinitely until server sends it that it’s process on server has been exited, and then it will disconnect and cancel the thread 1 and exiting itself. It is joined to main thread. It can exit the whole process if user wrote exit command.
  - **Main thread**: It waits for thread 2 to exit and the prompt the user again for connection.It will block to wait for thread 2 to exit. The thread 2 either exits the whole program, or exits itself and cancel the thread 1.
  - The process can only be exited if the user typed exit command. In that case, thread 2 will call exit. If the user typed disconnect , itwill only exit the two threads and main thread will survive.
  
  
