#include "operations.h"
#include "tracker_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>

void* tracker_console(void* arg){
    char cmd[BUFFER_SIZE];
    while(tracker_running){
        if(fgets(cmd,BUFFER_SIZE, stdin)==NULL) continue;
        cmd[strcspn(cmd, "\n")]=0;
        if(strcmp(cmd,"quit")==0){
            printf("Tracker %d shutting down...\n",tracker_no);
            tracker_running = 0;
            break;
        }
    }
    return nullptr;
}

int main(int argc, char* argv[]){
    if(argc != 3){
        printf("Usage: %s tracker_info.txt tracker_no\n",argv[0]);
        return 0;
    }
    tracker_no = atoi(argv[2]);
    if(tracker_no <1 || tracker_no>2){
        fprintf(stderr,"invalid tracker number. It must be 1 or 2\n");
        return 1;
    }
    char tracker_file[256];
    snprintf(tracker_file, sizeof(tracker_file),"../%s",argv[1]);

    FILE* f = fopen(tracker_file,"r");
    if(!f) {
        perror("fopen");
        return 1;
    }
    for(int i=0;i<2;i++)
        fscanf(f,"%s %d %d",tracker_ips[i],&client_ports[i],&sync_ports[i]);
    fclose(f);

    pthread_t console_thread;
    pthread_create(&console_thread,nullptr,tracker_console,nullptr);
    pthread_detach(console_thread);

    pthread_t sync_thread;
    int* pport = (int*)malloc(sizeof(int));
    *pport= sync_ports[tracker_no-1];
    pthread_create(&sync_thread,nullptr,tracker_sync_listener,pport);
    pthread_detach(sync_thread);

    int sockfd=socket(AF_INET, SOCK_STREAM,0);
    struct sockaddr_in addr{},cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(client_ports[tracker_no -1]);
    bind(sockfd,(struct sockaddr*)&addr, sizeof(addr));
    listen(sockfd,5);
    printf("Client listener on port %d\n",client_ports[tracker_no-1]);

    int flags = fcntl(sockfd, F_GETFL,0);
    fcntl(sockfd,F_SETFL,flags|O_NONBLOCK);

    while(tracker_running){
        int new_sock = accept(sockfd, (struct sockaddr*)&cli_addr, &cli_len);
        if(new_sock >=0){
            int* pclient = (int*)malloc(sizeof(int));
            *pclient = new_sock;
            pthread_t th;
            pthread_create(&th,nullptr,client_handler,pclient);
            pthread_detach(th);
        }else usleep(100000);
    }
    close(sockfd);
    printf("Tracker %d exited\n",tracker_no);
    return 0;
}