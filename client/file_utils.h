#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <utility>
#include <cstdint>

using namespace std;

struct DownloadState {
    string dest_path;
    uint64_t filesize;
    int total_pieces;
    int completed;
    bool done;
    vector<int> piece_status; 
    string whole_sha1;
    vector<string> piece_hashes;
    vector<pair<string,int>> seeders; 
    mutex mtx;
};

extern int PIECE_SIZE;
extern map<string,pair<string,uint64_t>> local_seeding_files;
extern map<string,shared_ptr<DownloadState>> ongoing_downloads;

string sha1_file(const string& path);
vector<string> piece_hashes(const string& path);
void peer_listener_thread(int peer_port);
void handle_peer_connection(int client_fd);
bool download_piece_from_seeder(const string &peer_ip, int peer_port,const string &gid, const string &filename, int idx, vector<char> &out_data);
void upload_file(int tracker_sock, const string& gid, const string& filepath,const string &my_ip, int my_port);



#endif

