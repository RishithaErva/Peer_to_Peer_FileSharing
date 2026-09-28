#ifndef TRACKER_SERVER_H
#define TRACKER_SERVER_H

#include <string>
#include <netinet/in.h> 
#include <arpa/inet.h>

#define BUFFER_SIZE 1024


struct client_info {
    int sock;
    struct sockaddr_in addr;
};

extern int tracker_running;
extern int tracker_no;
extern char tracker_ips[2][50];
extern int client_ports[2];
extern int sync_ports[2];


void* client_handler(void* arg);
void* tracker_sync_listener(void* arg);
void send_sync(const char* msg);

#endif
