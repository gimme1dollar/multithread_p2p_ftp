#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 30

int main(int argc, char *argv[])
{
	// Argument check
	if (argc != 3) {
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1);
	}
	
	// Socket instiantiation
	int clnt_sock;
	pid_t pid;
	char buf[BUF_SIZE];
	struct sockaddr_in clnt_addr;

	clnt_sock = socket(PF_INET, SOCK_STREAM, 0);  
	memset(&clnt_addr, 0, sizeof(clnt_addr));
	clnt_addr.sin_family = AF_INET;
	clnt_addr.sin_addr.s_addr = inet_addr(argv[1]);
	clnt_addr.sin_port = htons(atoi(argv[2]));
	
	// try connect
	if (connect(clnt_sock, (struct sockaddr*)&clnt_addr, sizeof(clnt_addr)) == -1)
		printf("connect() error!");

	printf("Welcome!\n");
	while(1) {
		memset(buf, 0x00, sizeof(buf));
                printf("write : ");

                fgets(buf, sizeof(buf), stdin);
                buf[strlen(buf)-1] = '\0';

                if(write(clnt_sock, buf, sizeof(buf)) <= 0)
                {
                        close(clnt_sock);
                        break;
                }

                memset(buf, 0x00, sizeof(buf));
                if (read(clnt_sock, buf, sizeof(buf)) <= 0)
                {
                        close(clnt_sock);
                        break;
                }

                printf("read : %s\n", buf);
	}
}
