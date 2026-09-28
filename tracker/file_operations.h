#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include<string>
#include<vector>
#include<unordered_map>
#include<unordered_set>
#include<cstdint>
#include<mutex>

using namespace std;

struct SeederInfo{
    string ip;
    int port;
    bool operator==(const SeederInfo& other) const {
        return ip==other.ip && port == other.port;
    }
};

namespace std {
    template<>
    struct hash<SeederInfo> {
        size_t operator()(const SeederInfo& s) const {
            return hash<string>()(s.ip) ^ hash<int>()(s.port);
        }
    };
}

struct FileInfo{
    string filename;
    string owner;
    string groupid;
    uint64_t filesize;
    string file_sha1;
    vector<string> piece_sha1;
    unordered_set<SeederInfo> seeders;
};

extern unordered_map<string, unordered_map<string, FileInfo>> group_files;
extern mutex group_files_mtx;

void handle_upload_file(const string& gid, const string& filename, const string& owner,uint64_t filesize, const string& file_sha1,const vector<string>& piece_hashes,const string& seeder_ip, int seeder_port, string& msg);

#endif
