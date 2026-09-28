#include "file_operations.h"
#include<string>
#include<vector>
#include<iostream>
#include <mutex>
#include <unordered_map>

using namespace std;

unordered_map<string, unordered_map<string, FileInfo>> group_files;
mutex group_files_mtx;

void handle_upload_file(const string& gid, const string& filename, const string& owner, uint64_t filesize, const string& file_sha1, const vector<string>& piece_hashes, const string& seeder_ip, int seeder_port,string& msg){
    lock_guard<mutex> lock(group_files_mtx);
    if(group_files[gid].find(filename)==group_files[gid].end()){
        FileInfo fi;
        fi.filename = filename;
        fi.owner = owner;
        fi.groupid = gid;
        fi.filesize = filesize;
        fi.file_sha1 = file_sha1;
        fi.piece_sha1 = piece_hashes;
        if(!seeder_ip.empty() && seeder_port > 0){
            fi.seeders.insert({seeder_ip, seeder_port});
        }
        group_files[gid][filename] = fi;
        msg = "File uploaded and you are seeder\n";
    }else{
        if(!seeder_ip.empty() && seeder_port > 0){
            group_files[gid][filename].seeders.insert({seeder_ip, seeder_port});
            msg = "Added as seeder for existing file.\n";
        } else {
            msg = "File already exists in group " + gid + "\n";
        }
    }
}