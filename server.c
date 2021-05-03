#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define MAX_CLIENT 5
#define BUF_SIZE 30

void *t_function(void *data);
int client_index = 0;

int main(int argc, char *argv[])
{
	// Argument check
	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	// Socket instiantiation (with thread)
	int serv_sock, clnt_sock;
	pthread_t thread_client[MAX_CLIENT];

	struct sockaddr_in serv_adr, clnt_adr;
	socklen_t adr_sz;
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));
	
	// Socket binding
	if (bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1)
		printf("bind() error\n");

	// Socket listening
	if (listen(serv_sock, 5) == -1)
		printf("listen() error\n");

	// Variables for messages
	int str_len;
	char msg[BUF_SIZE];
	char* buf = malloc(sizeof(char)*BUF_SIZE);

	while(1) {
		printf("accepting client...\n");

		clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
		if(clnt_sock == -1) {
			printf("accept error");
			exit(1);
		}

		if(client_index >= MAX_CLIENT) {
                        printf("client accept full (max client count : %d)\n", MAX_CLIENT);
                        close(clnt_sock);
                        continue;
                }

		if(pthread_create(&thread_client[client_index], NULL, t_function, (void *)&clnt_sock) != 0) {
                        printf("thread create error\n");
                        close(clnt_sock);
                        continue;
		}

		client_index++;
                printf("client accepted (Addr: %s, Port: %d)\n", inet_ntoa(clnt_adr.sin_addr), ntohs(clnt_adr.sin_port));
	}
}

void *t_function(void *arg) {
        int clnt_sock = *((int *)arg);
        pid_t pid = getpid(); // process id
        pthread_t tid = pthread_self(); // thread id
        printf("pid:%u, tid:%x\n", (unsigned int)pid, (unsigned int)tid);

        char buf[BUF_SIZE];
        while(1)
        {
                memset(buf, 0x00, sizeof(buf));
                if (read(clnt_sock, buf, sizeof(buf)) <= 0) {
                        printf("client %d exit\n", clnt_sock);
                        client_index--;
                        close(clnt_sock);
                        break;
                }
                printf("read : %s\n", buf);

                if(write(clnt_sock, buf, sizeof(buf)) <= 0) {
                        printf("client %d exit\n", clnt_sock);
                        client_index--;
                        close(clnt_sock);
                        break;
                }
                printf("write : %s\n", buf);
        }
}
