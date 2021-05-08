#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>

#define BUF_SIZE 30
#define MAX_CLNT 10
#define MAX_FILE 10

void *handle_clnt(void *arg);
void send_msg(char *msg, int len);

int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
pthread_mutex_t mtx;


int main(int argc, char *argv[])
{
	// Argument check
	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	// Thread
	pthread_t tid;
	pthread_mutex_init(&mtx, NULL);
	
	// Socket instiantiation 
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_addr, clnt_addr;
	socklen_t clnt_addr_sz;
	
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (serv_sock < 0) {
		printf("server socket() error\n");
		exit(1);
	}
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));
	
	// Socket binding
	if (bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1) {
		printf("bind() error\n");
		exit(1);
	}
	
	// Socket listening
	if (listen(serv_sock, 5) == -1) {
		printf("listen() error\n");
		exit(1);
	}

	// Variables for messages
	int str_len;
	char msg[BUF_SIZE];
	char* buf = malloc(sizeof(char)*BUF_SIZE);

	// clients
	while(1) {
		printf("accepting client...\n");

		clnt_addr_sz = sizeof(clnt_addr);
		clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_sz);
		
		if(clnt_sock < 0) {
			printf("client accept error");
			exit(1);
		}

		if(clnt_cnt >= MAX_CLNT) {
                        printf("client accept full (max client count : %d)\n", MAX_CLNT);
                        close(clnt_sock);
                        continue;
                }

		pthread_mutex_lock(&mtx);
		clnt_socks[clnt_cnt++] = clnt_sock;
		pthread_mutex_unlock(&mtx);
		
		if(pthread_create(&tid, NULL, handle_clnt, (void *)&clnt_sock) != 0) {
                        printf("thread create error\n");
                        close(clnt_sock);
                        continue;
		}
		pthread_detach(tid);
		printf("client accepted (Addr: %s, Port: %d)\n", inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));
	}
	
	close(serv_sock);
	return 0;
}

void *handle_clnt(void *arg) {
        int clnt_sock = *((int *)arg);
        
        pid_t pid = getpid(); // process id
        pthread_t tid = pthread_self(); // thread id
        printf("pid:%u, tid:%x c_sock:%d\n", (unsigned int)pid, (unsigned int)tid, clnt_sock);

        char msg[BUF_SIZE];
        int str_len = 0;
        
        // send directory list
        char filedir[BUF_SIZE] = "./serv_repo/\0";
        char filelist[BUF_SIZE*MAX_FILE] = "server repo list:\n";
        
        DIR *dir = NULL;
        struct dirent *ent;
        if ((dir=opendir(filedir)) != NULL) {
        	while((ent = readdir(dir)) != NULL) {
        		if(!strcmp( ent->d_name, ".")) continue;
        		if(!strcmp( ent->d_name, "..")) continue;
        		strcat(filelist, " \0");
			strcat(filelist, ent->d_name);
			strcat(filelist, "\n");
        	}
        	closedir(dir);
        } else {
        	printf("No directory %s\n", filedir);
        }
        write(clnt_sock, filelist, sizeof(filelist));
        
        // read routine
        while(str_len = read(clnt_sock, msg, BUF_SIZE) != 0) {
        	printf("%s", msg);
        	send_msg(msg, str_len);
        }
        
        // client closing
        printf("client %d closing\n", clnt_sock);
        pthread_mutex_lock(&mtx);
        for (int i = 0; i < clnt_cnt; i++) {
        	if(clnt_sock == clnt_socks[i]) {
        		while(++i < clnt_cnt) {
        			clnt_socks[i-1] = clnt_socks[i];
        		}
        		break;
        	}
        }
        clnt_cnt--;
        pthread_mutex_unlock(&mtx);
        close(clnt_sock);
        
        return NULL;
}

void send_msg(char *msg, int len) {
	int trgt_sock;
	
	for(int i = 0; i < clnt_cnt; i++) {
		pthread_mutex_lock(&mtx);
		trgt_sock = clnt_socks[i];
		pthread_mutex_unlock(&mtx);
		
		write(trgt_sock, msg, BUF_SIZE);
	}
}
