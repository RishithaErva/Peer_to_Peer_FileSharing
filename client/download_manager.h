#ifndef DOWNLOAD_MANAGER_H
#define DOWNLOAD_MANAGER_H

#include <string>

using namespace std;

void start_download(int tracker_sock, const std::string &gid, const string &filename,const string &dest_path,const string &my_ip,int my_port);
void show_downloads();
void stop_share(const string &gid,const string &filename,string &msg);

#endif

