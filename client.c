#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>

#define ERRARG "Please enter ip address and port to which you want to connected\nSyntax: ./milestone3-server localhost portnumber\n"
pthread_t t1,t2;
int sockfd=-1;
void
sig_handler(int signo)
{
	if(signo==SIGINT){
		if(sockfd!=-1){
			if(write(sockfd,"5\n exit\n",8)<0)
				perror("Sighand:write:");
		}else{
			exit(0);
		}
	}

}
static void *
thread1(void *sock){
	int *s=(int*)sock;
	
	while(1==1){
		char str1[100];
		char str2[3];
		int rc = read(0,str1,100); 
		if(rc<0)
			perror("T1:read from screen") ;
		str1[rc]='\0';

		if(rc==1)
		    continue;
		    
		if(sprintf(str2,"%d\n",rc)<0)
			perror("T1:sprintf:");
		if(write(*s,str2,3)<0)
			perror("T1:write length to sock:");
		if(write(*s,str1,strlen(str1))<0)
			perror("T1:write cmd to sock:");
	}
	return NULL;
}

static void *
thread2(void *sock){
	int *s=(int*)sock;
	while(1==1){
	
		char str[5000];		
			
		int rc;
		rc=read(*s,str,5000);
		if(rc==-1)
			perror("T2:read from sock:");
		str[rc]='\0';
		
		if(write(1,str,rc)!=rc){
			perror("T2:write to screen:");
		}
		if(strcmp(str,"Client Terminated\n")==0)
			exit(0);
		
		if(strcmp(str,"Client Disconnected\n")==0){
			sockfd=-1;
			pthread_cancel(t1);
			pthread_exit(NULL);
		}
	}
	return NULL;
}
int 
main (int argc, char *argv[])
{	
	//signal(SIGINT,sig_handler);
	char *ip,*port;  	
	while(1==1){
		char str[50];
		int read_count = read(1,str,50);     
		str[read_count]='\0';

		char* token = strtok(str, " ,\n");
		if(token!=NULL && strcmp(token,"connect")==0){
			token=strtok(NULL," \n");
			if(token!=NULL){
				ip=token;
				token=strtok(NULL," \n");
				if(token!=NULL){
					port=token;
				}
				else{
					if(write(1,"Port Number Missing\n",20)<0)
						perror("Port Number Print:");
					continue;
				}
			}else{
				if(write(1,"IP Address Missing\n",19)<0)
					perror("IP Print:");
				continue;
			}
		}
		else if(token!=NULL && strcmp(token,"exit")==0){
			exit(0);
		}
		else{
			continue;
		}
		
		
		struct sockaddr_in server;
		struct hostent *hp;

		/* Create socket */
		sockfd = socket(AF_INET, SOCK_STREAM, 0);

		if (sockfd < 0) {
			perror("opening stream socket");
			continue;
		}
		/* Connect socket using name specified by command line. */
		server.sin_family = AF_INET;
		hp = gethostbyname(ip);
		if (hp == 0) {
			fprintf(stderr, "%s: unknown host\n", argv[1]);
			continue;
		}
		bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
		server.sin_port = htons(atoi(port));

		if (connect(sockfd,(struct sockaddr *) &server,sizeof(server)) < 0) {
			perror("connecting stream socket");
			continue;
		}
		
		write(1,"Connected.\n",11);
		pthread_create(&t1, NULL,thread1,(void *)&sockfd);
		pthread_create(&t2, NULL,thread2,(void *)&sockfd);

		pthread_join(t2,NULL);	
	}	

}

