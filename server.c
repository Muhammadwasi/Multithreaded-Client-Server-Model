#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <sys/poll.h>
#include <arpa/inet.h>

#define ERRINV "Invalid Command\n"
#define ERRINC "Incomplete Command\n"
#define CLITERM "Client Terminated\n"
#define CLIDISCONN "Client Disconnected\n"
#define HELP "add:	Syntax: add [number1] [number2] ...\n\nsub:	Syntax: sub [number1] [number2] ...\n\nmult:	Syntax: mult [number1] [number2] ...\n\ndiv:	Syntax: div [number1] [number2] ...\n\nrun:	Syntax: run [program] [filename1] [filename2] ...\n\nlist:	Syntax: list [all]\n\nexit:	Syntax: exit\n\nexit:	Syntax: kill [Process Id]\n\n"

//#define LISTALL "--------------------------------------------------------------------------\n  Process Name   |  PId  | Status | StartTime | End Time | ElapsedTime\n--------------------------------------------------------------------------\n"

//#define LIST "-------------------------------------------------\n  Process Name   |  PId  | Status | StartTime \n-------------------------------------------------\n
void add(int);
void sub(int);
void mult(int);
void divide(int);
void list(int);
void sig_handler_client(int);
void sig_handler_server(int);
void run(int);
void killProcess(int);
void listConns();
void listAll(int);
void killAll();
struct conn{
	int connId;
	char ip[INET_ADDRSTRLEN];
	int port;
	int pid;
	int rfd;//server read fd
	int wfd;//client write fd
	int status;
};
struct processInfo{
	char name[50];
	int pid;
	int active;
	time_t st_t;
	time_t et_t;
	int elapsedTime;
};

int processCount=0;
int connCount=0;
int is_from_server=0;
struct processInfo pinfo[100];
struct conn conns[500];
char answer[100];
int result = 0;
int number;
char *token;
int p1[2];
int p2[2];
const char listAllHeader[]= "--------------------------------------------------------------------------\n  Process Name   |  PId  | Status | StartTime | End Time | ElapsedTime\n--------------------------------------------------------------------------\n\0";
const char listHeader[]="-------------------------------------------------\n  Process Name   |  PId  | Status | StartTime \n-------------------------------------------------\n";
const char connListHeader[]="-----------------------------------------------\nConn Id |  IP Address  |  Port |  Pid  | Status \n-----------------------------------------------\n\0";

int 
main (int argc, char *argv[])
{
	
	signal(SIGCHLD,sig_handler_server);
	
	int ser_sock, ser_len,cli_len,msgsock,ser_port,cli_port;
	
	struct sockaddr_in server,client;
	
	char server_ip[INET_ADDRSTRLEN],client_ip[INET_ADDRSTRLEN];

	cli_len=sizeof(client);

	/* Create socket */
	ser_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (ser_sock < 0) {
		perror("opening stream socket");
		exit(1);
	}
	/* Name socket using wildcards */
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = (INADDR_ANY);
	server.sin_port = 0;
	
	
	if (bind(ser_sock, (struct sockaddr *) &server, sizeof(server))) {
		perror("binding stream socket");
		exit(1);
	}
	/* Find out assigned port number and print it out */
	ser_len = sizeof(server);
	if (getsockname(ser_sock, (struct sockaddr *) &server, (socklen_t*) &ser_len)) {
		perror("getting socket name");
		exit(1);
	}

	inet_ntop(AF_INET, &(server.sin_addr), server_ip, INET_ADDRSTRLEN);
	ser_port=ntohs(server.sin_port);
	printf("Socket has port #%d\n",ser_port );
	printf("Socket has ip %s\n",server_ip);
	fflush(stdout);
	/* Start accepting connections */
	
	listen(ser_sock, 5);
	//p1[0]--->read fd
	//p2[1]--->write fd
	struct pollfd fds[2];
	int ready ;
	fds[0].fd=ser_sock;
	fds[0].events=POLLIN;
	
	fds[1].fd=0;
	fds[1].events=POLLIN;
	
	
	do {	
		int ret=poll(fds,2,-1);
	 	if(ret<0){
	 		//perror("Poll in Server:");	
	 		if(errno==EINTR)
				ret=poll(fds,2,-1);
	 	}	
 		if(fds[1].revents==POLLIN && fds[1].fd==0){
 			char buff3[100],result[100];
 			int read_count=read(0,buff3,sizeof(buff3));
 			buff3[read_count]='\0';
 			if(read_count==1)
 				continue;
			char* token = strtok(buff3, " ,\n");
			   
			if(strcmp(token,"list")==0){
				token=strtok(NULL," ,\n");
				if(token!=NULL && strcmp(token,"conn")==0){
					listConns();
				}
				if(token!=NULL && strcmp(token,"process")==0){
					
					char pBuff[5000];
					pBuff[0]='\0';
				
					strcpy(pBuff,"No Connections\n");
					for(int i =0;i<connCount;i++){
						if(i==0)
								pBuff[0]='\0';
						if(conns[i].status==1){
							
						
							char temp[1000];
							int r=sprintf(temp,"				Conn Id: %2d\n",conns[i].connId);
							temp[r]='\0';
							strcat(pBuff,temp);
							temp[0]='\0';
							if(write(conns[i].wfd,"list",4)<0)
								perror("write error");
							r=read(conns[i].rfd,temp,sizeof(temp));
							if(r<0)
								perror("read error");
							temp[r]='\0';
							strcat(pBuff,temp);
							}
						}
						strcat(pBuff,"\0");
					write(1,pBuff,strlen(pBuff));
				}
			}
			else if(strcmp(token,"exit")==0){
				for(int i=0; i<connCount;i++){
					if(conns[i].status==1){
						if(write(conns[i].wfd,"exit",4)<0)
							perror("write error");
						write(1,"ending a client\n",16);
					}
				}
				
				exit(0);
			}else{
				write(1,ERRINV,strlen(ERRINV));
			}
 		}

 		if(fds[0].revents==POLLIN && fds[0].fd==ser_sock){
 			msgsock = accept(ser_sock, (struct sockaddr *)&client, &cli_len);
			if (msgsock == -1){
				perror("accept");
				continue;
			}
			inet_ntop(AF_INET, &(client.sin_addr), client_ip, INET_ADDRSTRLEN);
			cli_port=ntohs(client.sin_port);
			printf("Starting connection with %s\n",client_ip);
			if(pipe(p1)<0)
				perror("pipe");
			if(pipe(p2)<0)
				perror("pipe");
			int f=fork();
			
			
			if(f>0){
				conns[connCount].connId=connCount;
				conns[connCount].pid=f;
				strcpy(conns[connCount].ip,client_ip);
				conns[connCount].port=ser_port;	
				conns[connCount].wfd=p1[1];
				conns[connCount].rfd=p2[0];
				conns[connCount].status=1;
				connCount++;
				close(p1[0]);
				close(p2[1]);
			}
			if(f==0){
				close(p2[0]);//p2[0]-->read fd
				close(p1[1]);//p1[1]-->write fd
				signal(SIGCHLD,sig_handler_client);
				struct pollfd fds2[2];
				int ready2;
				fds2[0].fd=msgsock;
				fds2[0].events=POLLIN;
	
				fds2[1].fd=p1[0];
				fds2[1].events=POLLIN;
			
				while(1==1){
			 		int ret2=poll(fds2,2,-1);
			 		if(ret2<0){
	 					//perror("Poll in Client:");	
	 				}
			 		if(fds2[0].revents==POLLIN && fds2[0].fd==msgsock){
			 			char firstCount[3];
						int rc=read(msgsock,firstCount,3);
						char *t=strtok(firstCount," \n");
						int count=(int)atoi(t);
						char str[100];
					   	int read_count = read(msgsock,str,count);
					   
						str[read_count]='\0';
					   	char* token = strtok(str, " ,\n");
						if(strcmp(token,"add")==0)
							add(msgsock);	
						else if(strcmp(token,"sub")==0)
							sub(msgsock);
						else if(strcmp(token,"mult")==0)
							mult(msgsock);
						else if(strcmp(token,"div")==0)
						 	divide(msgsock);				
						else if(strcmp(token,"help")==0){
							token = strtok(NULL, " ,\n");
							if(token==NULL)
							 	write(msgsock,HELP, strlen(HELP));
							 else
							 	write(msgsock,ERRINV,strlen(ERRINV));
							 
						}
						else if (strcmp(token,"run")==0)
						 	run(msgsock);
						else if(strcmp(token,"exit")==0){
						 	write(msgsock,CLITERM, strlen(CLITERM));
						 	write(1,CLITERM, strlen(CLITERM));
						 	killAll();
							close(msgsock);
						 	exit(0);
						 }
						else if(strcmp(token,"disconnect")==0){
						 	write(msgsock,CLIDISCONN, strlen(CLIDISCONN));
						 	write(1,CLIDISCONN, strlen(CLIDISCONN));
						 	killAll();
							close(msgsock);	
						 	exit(0);

						 }
						else if(strcmp(token,"list")==0){
						 	token = strtok(NULL, " ,\n");
						 	if(token!=NULL && strcmp(token,"all")==0){
						 		token = strtok(NULL, " ,\n");
						 		if(token==NULL)
						 			listAll(msgsock);
						 		else
						 			write(msgsock,ERRINV,strlen(ERRINV));
						 	}else if(token==NULL){
						 		list(msgsock);
						 	}else{
						 		write(msgsock,ERRINV,strlen(ERRINV));
						 	}
						}
						else if(strcmp(token,"kill")==0)
							killProcess(msgsock);
				
						else
						 	write(msgsock,ERRINV,strlen(ERRINV));
					
						}
						if(fds2[1].revents==POLLIN &&  fds2[1].fd==p1[0]){
							char t[10];
							int rcount=read(p1[0],t,4);
							if(rcount==-1)
								perror("read error:");
							t[rcount]='\0';
							if(strcmp(t,"list")==0)
								listAll(p2[1]);
							else if (strcmp(t,"exit")==0){
								write(msgsock,CLITERM, strlen(CLITERM));
						 		write(1,CLITERM, strlen(CLITERM));
						 		killAll();
								close(msgsock);
						 		exit(0);
							}
								
						}
					}
				}	
 			}
		}while(1==1);	
}

//--------------------------------------------------------------------------------------------------------------------------------
void add(int msgsock){
	char answer[100];
	result=0;
	char *token = strtok(NULL, " ,\n");
	while(token!=NULL){
		number = (int)atoi(token);
		result = result + number;
		token = strtok(NULL, " ,\n");
	}
	sprintf(answer, "%d\n", result);
	write(msgsock,answer,strlen(answer));
}

void sub(int msgsock){
	char answer[100];
	result=0;
	token = strtok(NULL, " ,\n");
	if(token!=NULL){
		number = (int)atoi(token);
		result = number;
	}
	else{
		write(msgsock,ERRINC,strlen(ERRINC));
		return;
	}
	token = strtok(NULL, " ,\n");
	while(token!=NULL){
		number = (int)atoi(token);
		result = result-number;
		token = strtok(NULL, " ,\n");
	}
	sprintf(answer, "%d\n", result);
	write(msgsock,answer,strlen(answer));
}

void mult(int msgsock){
	char answer[100];
	token = strtok(NULL, " ,\n");
	
	if(token!=NULL){
		number = (int)atoi(token);
		result = number;
	}
	else{
		write(msgsock,ERRINC,strlen(ERRINC));
		return;
	}
	token = strtok(NULL, " ,\n");
	while(token!=NULL){
		number = (int)atoi(token);
		result = result*number;
		token = strtok(NULL, " ,\n");
	}
	sprintf(answer, "%d\n", result);
	write(msgsock,answer,strlen(answer));


}

void divide(int msgsock){
	char answer[100];
	float result;
	token = strtok(NULL, " ,\n");
	if(token!=NULL){
		number = (int)atoi(token);
		result = number;
	}
	else{
		write(msgsock,ERRINC,strlen(ERRINC));
		return;
	}
	token = strtok(NULL, " ,\n");
	while(token!=NULL){
		number = (int)atoi(token);
		result = result/number;
		token = strtok(NULL, " ,\n");
	}
	sprintf(answer, "%0.4f\n", result);
	write(msgsock,answer,strlen(answer));


}
void listAll(int msgsock){
	int i;
 	char buff[5000];

 	char temp[100];

 	strcpy(buff,listAllHeader);
 	
	for(i=0;i<processCount;i++){
		
 			struct tm *st=localtime(&(pinfo[i].st_t));
	 		sprintf(temp,"%16s | %5d | %6d | %02d:%02d:%02d  ",pinfo[i].name,pinfo[i].pid,pinfo[i].active,
	 		st->tm_hour,st->tm_min,st->tm_sec);
	 		strcat(temp,"\0");
	 		strcat(buff,temp);
 			if(pinfo[i].active!=1){
				struct tm *et=localtime(&(pinfo[i].et_t));
		 		sprintf(temp,"| %02d:%02d:%02d |   %02dm %02ds   ",et->tm_hour,et->tm_min,et->tm_sec,
		 		pinfo[i].elapsedTime/60,pinfo[i].elapsedTime%60);
		 		strcat(temp,"\0");
		 		strcat(buff,temp);
 			}
			strcat(buff,"\n");
			
 	}
 	strcat(buff,"\0");
 	write(msgsock,buff,strlen(buff));
}

void list(int msgsock){
	int i;
 	char buff2[5000];
 	char temp[100];
 	strcpy(buff2,listHeader);

 	for(i=0;i<processCount;i++){
 		if(pinfo[i].active==1){
 		struct tm *st=localtime(&(pinfo[i].st_t));
	 		sprintf(temp,"%16s | %5d | %6d | %02d:%02d:%02d  \n",pinfo[i].name,pinfo[i].pid,pinfo[i].active,
	 		st->tm_hour,st->tm_min,st->tm_sec);
	 		strcat(buff2,temp);
 		}
 	}
 	strcat(buff2,"\0");
 	write(msgsock,buff2,strlen(buff2));
}
void sig_handler_client(int signo){
	if(signo==SIGCHLD){
		int status,count;
		int pid=waitpid(-1,&status,WNOHANG);
		while(pid>0){
			count=0;
			while(count<processCount){
				if(pinfo[count].pid==pid){
					pinfo[count].active=0;
					pinfo[count].et_t=time(NULL);
					pinfo[count].elapsedTime=pinfo[count].et_t-pinfo[count].st_t;
				}
				count++;
			}
			pid=waitpid(-1,&status,WNOHANG);
		}
	}
	
}

void run(int msgsock){
	char *mylist[10];
	int count=0;
	token = strtok(NULL, " ,\n");
	if(token!=NULL){
		while(token!=NULL){
	 		mylist[count++]=token;
	 		token = strtok(NULL, " ,\n");
	 	}
	 	mylist[count]=NULL;
	 	
	 	//creating new process child
		int fd[2];
		if(pipe(fd)<0){
			write(msgsock,strerror(errno),strlen(strerror(errno)));
			return;
		}
		int flags=fcntl(fd[1],F_GETFD);
		flags |=FD_CLOEXEC;
		fcntl(fd[1],F_SETFD,flags);
	 	int pid=fork();
	 	
		if(pid==-1){
			write(msgsock,strerror(errno),strlen(strerror(errno)));
			return;
	 		
	 	}
	 	
	 	
	 	//------------New process Code------------------
		else if(pid==0){
			close(fd[0]);
			//if it fails then it will be exit with status 99 
			//status will be used in waitpid
			if(execvp(mylist[0],mylist)==-1){
				if(write(fd[1],strerror(errno),strlen(strerror(errno)))<0)
					perror("exec failed error:");
				exit(99);
			}

		}
		//------------Parent Process Code------------------	
		else if(pid>0){
				close(fd[1]);
				char temp[100];
				int rc=read(fd[0],temp,sizeof(temp));
				if(rc==-1){
					if(write(msgsock,strerror(errno),strlen(strerror(errno))))
						perror("run write msgsock error:");
					return;
				}
				else if(rc==0){
					strcpy(pinfo[processCount].name,mylist[0]);
					pinfo[processCount].pid=pid;
					pinfo[processCount].active=1;
					pinfo[processCount].st_t=time(NULL);
					processCount++;
					if(write(msgsock,"Success\n",8)<0)
						perror("run write success error:");
				}
				else if(rc>0){
					if(sprintf(temp,"%s\n",temp)<0)
						perror("sprintf error:");
					if(write(msgsock,temp,rc+1)<0)
						perror("run write error:");
				}		
	 	}
 	}
 	else{
		write(msgsock,ERRINC,strlen(ERRINC));
		return;
 	}
}
void killAll()
{	
	for(int i=0;i<processCount;i++){
			if(pinfo[i].active==1)
				kill(pinfo[i].pid,SIGTERM);
	}
}
void killProcess(int msgsock){
	token = strtok(NULL, " ,\n");
	if(token==NULL){
		write(msgsock,ERRINC,strlen(ERRINC));
	}else if(strcmp(token,"all")==0){
		for(int i=0;i<processCount;i++){
			if(pinfo[i].active==1)
				kill(pinfo[i].pid,SIGTERM);
			
		}
		write(msgsock,"All Processes terminated\n",25);
	}else{
		int found=0;
		for(int i=0;i<processCount;i++){
			if(strcmp(pinfo[i].name,token)==0){
				found=1;
				if(pinfo[i].active==1){
					kill(pinfo[i].pid,SIGTERM);
					write(msgsock,"Process terminated\n",19);
					return;
				}
				
			}
		}
		
		int kpid=atoi(token);
		found=0;
		for(int i=0;i<processCount;i++){
			if(pinfo[i].pid==kpid){
				found=1;
				if(pinfo[i].active==0){
					write(msgsock,"Already terminated\n",19);
				}else{
					kill(kpid,SIGTERM);
					write(msgsock,"Process terminated\n",19);
				}
				return;
			}
		}
		if(found==0){
			write(msgsock,"Process with the given name/pid is not found/running\n",53);
		}
	}
}

void listConns(){
	char buff4[5000];
	char temp[100];
	strcpy(buff4,connListHeader);

 		for(int i=0;i<connCount;i++){
 			sprintf(temp,"%7d | %12s | %05d | %5d | %d",conns[i].connId,conns[i].ip,conns[i].port,conns[i].pid,conns[i].status);
 			strcat(buff4,temp);
 			strcat(buff4,"\n");
 		}
 		write(0,buff4,strlen(buff4));	
}

void sig_handler_server(int signo){
	if(signo==SIGCHLD){
		int status,count;
		int pid=waitpid(-1,&status,WNOHANG);
		while(pid>0){
			count=0;
			while(count<connCount){
				if(conns[count].pid==pid){
					conns[count].status=0;
				}
				count++;
			}
			pid=waitpid(-1,&status,WNOHANG);
		}
	}
}

