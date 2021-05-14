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
#include <sys/mman.h>

#define BUF_SIZE 30
#define MAX_CLNT 10
#define MAX_FILE 10
#define UNT_FILE 128

int clnt_cnt = 0;
int *clnt_stage;
void *clnt_routine(void *arg);

char file_dir[BUF_SIZE] = "./serv_repo/\0";
int file_cnt = 0;
char **file_list;
int *file_chnks;
int *file_clnts;

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
	
	// File settings
	file_list = (char**) malloc ( sizeof(char*) * MAX_FILE );
	for(int i = 0; i < MAX_FILE; i++){
	    file_list[i] = (char*) malloc ( sizeof(char) * BUF_SIZE );
	}
	file_chnks = (int*) malloc ( sizeof(int*)*MAX_FILE );
	file_clnts = (int*) malloc ( sizeof(int*)*MAX_FILE );

	file_list = (char**) mmap(NULL, sizeof(char)*MAX_FILE*BUF_SIZE, PROT_WRITE|PROT_READ|PROT_EXEC, MAP_SHARED|MAP_ANON, -1, 0);
	file_clnts = (int*) mmap(NULL, sizeof(int)*MAX_FILE, PROT_WRITE|PROT_READ|PROT_EXEC, MAP_SHARED|MAP_ANON, -1, 0);
	file_chnks = (int*) mmap(NULL, sizeof(int)*MAX_FILE, PROT_WRITE|PROT_READ|PROT_EXEC, MAP_SHARED|MAP_ANON, -1, 0);
	
	pthread_mutex_lock(&mtx);
	printf("Repo file list \n");
        DIR *dir = NULL;
        struct dirent *ent;
        if ((dir=opendir(file_dir)) != NULL) {
        	while((ent = readdir(dir)) != NULL) {
        		if(!strcmp( ent->d_name, ".")) continue;
        		if(!strcmp( ent->d_name, "..")) continue;
			file_list[file_cnt] = ent->d_name;
			printf("%d: %s\n", file_cnt, file_list[file_cnt]);
        		file_cnt += 1;
        	}
        	closedir(dir);
        } else {
        	printf("No directory %s\n", file_dir);
        }
	pthread_mutex_unlock(&mtx);

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

	// clients
	clnt_stage = (int*) malloc (sizeof(int*)*(MAX_CLNT+5));
	clnt_stage = (int*) mmap(NULL, sizeof(int)*(MAX_CLNT+5), PROT_WRITE|PROT_READ|PROT_EXEC, MAP_SHARED|MAP_ANON, -1, 0);

	while(1) {
		printf("\n...accepting client...\n");

		clnt_addr_sz = sizeof(clnt_addr);
		clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_sz);
		
		if(clnt_sock < 0) {
			printf("...client accept error");
			exit(1);
		}

		if(clnt_cnt >= MAX_CLNT) {
                        printf("...client accept full (max client count : %d)\n", MAX_CLNT);
                        close(clnt_sock);
                        continue;
                }

		printf("...client %d accepted (Addr: %s, Port: %d)\n", 
			clnt_sock, inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));

		pthread_mutex_lock(&mtx);
		clnt_stage[clnt_sock] = 0;
		clnt_cnt += 1;
		pthread_mutex_unlock(&mtx);
		
		if(pthread_create(&tid, NULL, clnt_routine, (void *)&clnt_sock) != 0) {
                        printf("thread create error\n");
                        close(clnt_sock);
                        continue;
		}
		pthread_join(tid, NULL);
	}
	
	free(clnt_stage);
	free(file_list);
	free(file_chnks);
	free(file_clnts);
	close(serv_sock);
	return 0;
}

void *clnt_routine(void *arg) {
	int clnt_sock = *((int *)arg);
	char msg[BUF_SIZE];
        
	// thread info
        pid_t pid = getpid(); // process id
        pthread_t tid = pthread_self(); // thread id
	// handle client
	while(1) {
		pthread_mutex_lock(&mtx);
        	printf("\n*** handling c_sock %d in stage %d ***\n", clnt_sock, clnt_stage[clnt_sock]);


		if(clnt_stage[clnt_sock] == 0) { 	
			printf("** sending file lists (client %d) **\n", clnt_sock);
		
			// send file name list
			sprintf(msg, "%d", file_cnt);
			printf("sending %s file_names\n", msg);
			write(clnt_sock, msg, BUF_SIZE);
					
		        for(int i = 0; i < file_cnt; i++) {
				memset(msg, 0, BUF_SIZE);
				strcpy(msg, file_list[i]);
				printf("sending file_name %s\n", msg);
			        write(clnt_sock, msg, BUF_SIZE);
			}
			
			// move to next stage
			clnt_stage[clnt_sock] += 1;
		}

		printf("*** out handler client %d ***\n\n", clnt_sock);
		pthread_mutex_unlock(&mtx);
		
	}

        return NULL;
}
