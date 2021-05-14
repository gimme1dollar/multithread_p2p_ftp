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
#define UNT_FILE 128*8

int clnt_cnt = 0;
char **clnt_addr_list;
int *clnt_stage;
void *clnt_routine(void *arg);

char file_dir[BUF_SIZE] = "./serv_repo/\0";
int file_cnt = 0;
char **file_names;
int *file_chnks;
int *file_clnt_num;
int **file_clnt_list;

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
        DIR *dir = NULL;
        struct dirent *ent;

	file_chnks = (int*) malloc ( sizeof(int) * MAX_FILE );
	file_clnt_num = (int*) malloc ( sizeof(int) * MAX_FILE );
	file_names = (char**) malloc ( sizeof(char*) * MAX_FILE );
	file_clnt_list = (int**) malloc ( sizeof(int*) * MAX_FILE );
	for(int i = 0; i < MAX_FILE; i++){
	    file_names[i] = (char*) malloc ( sizeof(char) * BUF_SIZE );
	    file_clnt_list[i] = (int*) malloc ( sizeof(int) * MAX_CLNT );
	}

	file_names = (char**) mmap(NULL, sizeof(char)*MAX_FILE*BUF_SIZE, PROT_WRITE|PROT_READ|PROT_EXEC, MAP_SHARED|MAP_ANON, -1, 0);
	file_chnks = (int*) mmap(NULL, sizeof(int)*MAX_FILE, PROT_WRITE|PROT_READ|PROT_EXEC, MAP_SHARED|MAP_ANON, -1, 0);
	file_clnt_num = (int*) mmap(NULL, sizeof(int)*MAX_FILE, PROT_WRITE|PROT_READ|PROT_EXEC, MAP_SHARED|MAP_ANON, -1, 0);
	//file_clnt_list = (int**) mmap(NULL, sizeof(int)*MAX_FILE*MAX_CLNT, PROT_WRITE|PROT_READ|PROT_EXEC, MAP_SHARED|MAP_ANON, -1, 0);
	
	pthread_mutex_lock(&mtx);

	printf("Repo file list \n");
        if ((dir=opendir(file_dir)) != NULL) {
        	while((ent = readdir(dir)) != NULL) {
        		if(!strcmp( ent->d_name, ".")) continue;
        		if(!strcmp( ent->d_name, "..")) continue;
			file_names[file_cnt] = ent->d_name;
        		file_cnt += 1;
        	}
        	closedir(dir);
        } else {
        	printf("No directory %s\n", file_dir);
        }
		
	for(int i = 0; i < file_cnt; i++) {
		char *tmp_name = (char*) malloc ( sizeof(char*)*2*BUF_SIZE );
		strcpy(tmp_name, file_dir);
		strcat(tmp_name, file_names[i]);

		FILE* tmp_file;
		tmp_file = fopen(tmp_name, "r");
		fseek(tmp_file, 0, SEEK_END);
		int file_size = ftell(tmp_file);

		file_size /= UNT_FILE;
		file_size += 1;
		fclose(tmp_file);
		free(tmp_name);
		
		file_chnks[i] = file_size;
		file_clnt_num[i] = 0;

		printf("%d: %s (%d chunks)\n", i, file_names[i], file_chnks[i]);
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
	clnt_addr_list = (char**) malloc ( sizeof(char*) * (MAX_CLNT+5) );
	for(int i = 0; i < (MAX_CLNT+5); i++){
	    clnt_addr_list[i] = (char*) malloc ( sizeof(char) * BUF_SIZE );
	}
	clnt_stage = (int*) malloc (sizeof(int*)*(MAX_CLNT+5));

	clnt_addr_list = (char**) mmap(NULL, sizeof(char)*(MAX_CLNT+5)*BUF_SIZE, PROT_WRITE|PROT_READ|PROT_EXEC, MAP_SHARED|MAP_ANON, -1, 0);
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
		clnt_addr_list[clnt_sock] = inet_ntoa(clnt_addr.sin_addr);
		clnt_cnt += 1;
		pthread_mutex_unlock(&mtx);
		
		if(pthread_create(&tid, NULL, clnt_routine, (void *)&clnt_sock) != 0) {
                        printf("...thread create error\n");
                        close(clnt_sock);
                        continue;
		}
		pthread_detach(tid);
	}
	
	free(clnt_addr_list);
	free(clnt_stage);
	free(file_names);
	free(file_chnks);
	free(file_clnt_num);
	close(serv_sock);
	return 0;
}

void *clnt_routine(void *arg) {
	int clnt_sock = *((int *)arg);

	// clnt status	
	int file_num = -1;
	int prev_file_clnt_num;

	// thread info
        pid_t pid = getpid(); // process id
        pthread_t tid = pthread_self(); // thread id
	printf("\n");

	// handle client
	while(1) {
		if(clnt_stage[clnt_sock] == 0) {
			char buf[BUF_SIZE];

			// send file name list
			sprintf(buf, "%d", file_cnt);
			printf("sending %s file_names\n", buf);
			write(clnt_sock, buf, BUF_SIZE);
					
		        for(int i = 0; i < file_cnt; i++) {
				sprintf(buf, "%d: %s (%d/%d)", i, file_names[i], file_clnt_num[i], file_chnks[i]);
				printf("sending file_name %s\n", file_names[i]);
			        write(clnt_sock, buf, BUF_SIZE);
			}
			printf("\n");
			
			// move to next stage
			clnt_stage[clnt_sock] += 1;
		} else if (clnt_stage[clnt_sock] == 1) { 
			char buf[BUF_SIZE];
			int str_len = 0;
	
			// send file name list
			str_len = read(clnt_sock, buf, BUF_SIZE);

			if(str_len > 0) {
				file_num = atoi(buf);

				printf("received file_idx %d from client %d\n", file_num, clnt_sock);
				printf("\n");
		

				if(file_num >= 0 && file_num <= file_cnt) {
					pthread_mutex_lock(&mtx);
					int clnt_num = file_clnt_num[file_num];
					file_clnt_num[file_num] = clnt_num + 1;
					file_clnt_list[file_num][clnt_num] = clnt_sock;
					pthread_mutex_unlock(&mtx);
				}

				// move to next stage
				clnt_stage[clnt_sock] += 1;
			}
		} else if (clnt_stage[clnt_sock] == 2) { 
			char buf[BUF_SIZE];

			pthread_mutex_lock(&mtx);
			int curr_file_clnt_num = file_clnt_num[file_num];
			int file_chnk = file_chnks[file_num];
			pthread_mutex_unlock(&mtx);

			// send client status of the chosen file
			if(curr_file_clnt_num != prev_file_clnt_num) {
				if(curr_file_clnt_num < file_chnk) {
					sprintf(buf, "waiting %s (%d/%d)\n", file_names[file_num], curr_file_clnt_num, file_chnk);
				} else {
					sprintf(buf, "complete %s (%d/%d)\n", file_names[file_num], curr_file_clnt_num, file_chnk);
					clnt_stage[clnt_sock] += 1;
				}
				write(clnt_sock, buf, BUF_SIZE);
				prev_file_clnt_num = curr_file_clnt_num;
			}
		} else if (clnt_stage[clnt_sock] == 3) { 
			char buf[BUF_SIZE];

			pthread_mutex_lock(&mtx);

			// send ip addresses of other clients
			int chnk_num = file_chnks[file_num];
			sprintf(buf, "%d", chnk_num-1);
			write(clnt_sock, buf, BUF_SIZE);
			
			for (int i = 0; i < chnk_num; i++) {
				int clnt = file_clnt_list[file_num][i];

				if(clnt != clnt_sock) {
					sprintf(buf, "%s", clnt_addr_list[clnt]);
					write(clnt_sock, buf, BUF_SIZE);
				}
			}
			printf("\n");


			clnt_stage[clnt_sock] += 1;
			pthread_mutex_unlock(&mtx);
		} else if (clnt_stage[clnt_sock] == 4) {
			char buf[BUF_SIZE];

			// send appropriate chunk (with idx) of the chosen file
			
		} else if (clnt_stage[clnt_sock] >= 4) {
			// set file_clnt_num value to 0
			pthread_mutex_lock(&mtx);
			file_clnt_num[file_num] = 0;
			pthread_mutex_unlock(&mtx);
			
			printf("\n... closing client %d\n", clnt_sock);
                        close(clnt_sock);
			break;
		} 
	}

        return NULL;
}
