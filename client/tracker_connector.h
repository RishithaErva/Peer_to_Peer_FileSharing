#ifndef TRACKER_CONNECTOR_H
#define TRACKER_CONNECTOR_H

#include <string>
using std::string;

struct TrackerInfo {
    string ip;
    int client_port;
    int sync_port;
};

extern TrackerInfo trackers[2]; 

int connect_to_tracker(const string &ip, int port);
int reconnect(int failed_index,int &tracker_index_ref);

#endif
