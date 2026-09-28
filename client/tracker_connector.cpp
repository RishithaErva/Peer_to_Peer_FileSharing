#include "tracker_connector.h"
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

TrackerInfo trackers[2];

int connect_to_tracker(const string &ip, int port){
    int sock = socket(AF_INET, SOCK_STREAM,0);
    if(sock<0){
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if(inet_pton(AF_INET, ip.c_str(), &addr.sin_addr)<=0){
        perror("inet_pton");
        close(sock);
        return -1;
    }

    if(connect(sock,(struct sockaddr*)&addr,sizeof(addr))<0){
        close(sock);
        return -1;
    }
    return sock;
}

int reconnect(int failed_index, int &tracker_index_ref){
    for(int i=0;i<2;i++){
        if(i==failed_index) continue;
        int sock = connect_to_tracker(trackers[i].ip,trackers[i].client_port);
        if(sock>=0){
            tracker_index_ref = i;
            cout<<"Connected to tracker "<<trackers[i].ip<<":"<<trackers[i].client_port<<endl;
            return sock;
        }
    }
    return -1;
}