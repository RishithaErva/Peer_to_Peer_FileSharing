#include "file_utils.h"
#include "download_manager.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <sstream>
#include <iomanip>
#include <memory>
#include <unistd.h>
#include <algorithm>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <sys/stat.h>

using namespace std;

string sha1_memory(const vector<char> &data){
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr);
    EVP_DigestUpdate(ctx, data.data(), data.size());
    EVP_DigestFinal_ex(ctx, md, &md_len);
    EVP_MD_CTX_free(ctx);

    stringstream ss;
    ss << hex << setfill('0');
    for(unsigned int i=0;i<md_len;i++) ss << setw(2) << (int)md[i];
    return ss.str();
}

void download_worker(const string &gid,const string &filename){
    string key = gid+":"+filename;
    auto it = ongoing_downloads.find(key);
    if(it == ongoing_downloads.end()) return;

    auto d_ptr = it->second;
    DownloadState &d = *d_ptr;

    while(true){
        int piece_idx=-1;
        {
            lock_guard<mutex> lock(d.mtx);
            if(d.completed>=d.total_pieces){ d.done=true; break; }
            for(int i=0;i<d.total_pieces;i++){
                if(d.piece_status[i]==0){ 
                    d.piece_status[i]=1; 
                    piece_idx=i; 
                    break; 
                }
            }
        }

        if(piece_idx==-1){ 
            this_thread::sleep_for(chrono::milliseconds(100)); 
            continue; 
        }

        bool success=false;
        for(auto &s:d.seeders){
            vector<char> data;
            if(download_piece_from_seeder(s.first,s.second,gid,filename,piece_idx,data)){
                string received_hash = sha1_memory(data);
                if(received_hash != d.piece_hashes[piece_idx]){
                    cout<<"Piece "<<piece_idx<<" hash mismatch. Expected: "
                        <<d.piece_hashes[piece_idx]<<" Got: "<<received_hash<<endl;
                    lock_guard<mutex> lock(d.mtx); 
                    d.piece_status[piece_idx]=0; 
                    break;
                }
                
                // Write piece to file
                ofstream f(d.dest_path, ios::binary|ios::in|ios::out);
                if(f){ 
                    f.seekp((long long)piece_idx*PIECE_SIZE); 
                    f.write(data.data(),data.size()); 
                    f.close();
                    lock_guard<mutex> lock(d.mtx); 
                    d.piece_status[piece_idx]=2; 
                    d.completed++;
                }
                success=true; 
                break;
            }
        }

        if(!success){ 
            lock_guard<mutex> lock(d.mtx); 
            d.piece_status[piece_idx]=0; 
            this_thread::sleep_for(chrono::milliseconds(500));
        }
    }

    lock_guard<mutex> lock(d.mtx);
    if(d.completed>=d.total_pieces){
        string final_sha1 = sha1_file(d.dest_path);
        if(final_sha1==d.whole_sha1){
            string final_path = d.dest_path.substr(0,d.dest_path.size()-5);
            rename(d.dest_path.c_str(), final_path.c_str());
            local_seeding_files[key]={final_path,d.filesize};
            cout<<"[C] ["<<gid<<"] "<<filename<<endl;
            cout<<"Download completed: "<<final_path<<endl;
        }else{
            cout<<"Download failed: SHA1 mismatch (expected: "<<d.whole_sha1
                <<", got: "<<final_sha1<<")"<<endl;
            remove(d.dest_path.c_str());
        }
        d.done=true;
    }
}

void start_download(int tracker_sock, const string &gid, const string &filename, 
                   const string &dest_path, const string &my_ip, int my_port){
    string key = gid+":"+filename;
    if(ongoing_downloads.count(key)){ 
        cout<<"Already downloading "<<key<<endl; 
        return; 
    }

    string req = "download_file " + gid + " " + filename + "\n";
    write(tracker_sock, req.c_str(), req.size());

    char buf[65536];
    int n = read(tracker_sock, buf, sizeof(buf)-1);
    if(n<=0){
        cout<<"Failed to get file info from tracker"<<endl;
        return;
    }
    buf[n]='\0';
    
    stringstream ss(buf);
    string line;
    getline(ss, line);

    if(line.find("not found") != string::npos || 
       line.find("No seeders") != string::npos ||
       line.find("Error") != string::npos){
        cout<<line<<endl;
        return;
    }
    uint64_t filesize;
    string whole_sha1;
    int num_pieces;
    stringstream meta(line);
    meta >> filesize >> whole_sha1 >> num_pieces;

    if(num_pieces <=0){
        cout<<"Invalid file metadata"<<endl;
        return;
    }
    vector<string> piece_hashes;
    for(int i=0; i<num_pieces; i++){
        string hash;
        getline(ss, hash);
        if(!hash.empty()) piece_hashes.push_back(hash);
    }

    if(piece_hashes.size() != (size_t)num_pieces){
        cout<<"Incomplete piece hashes received"<<endl;
        return;
    }
    string seeder_line;
    getline(ss, seeder_line);
    
    vector<pair<string,int>> seeders;
    stringstream seeder_ss(seeder_line);
    string seeder_info;
    while(seeder_ss >> seeder_info){
        size_t colon = seeder_info.find(':');
        if(colon != string::npos){
            string ip = seeder_info.substr(0, colon);
            int port = stoi(seeder_info.substr(colon+1));
            seeders.push_back({ip, port});
        }
    }

    if(seeders.empty()){
        cout<<"No seeders available for "<<filename<<endl;
        return;
    }

    cout<<"Starting download: "<<filename<<" ("<<filesize<<" bytes, "
        <<num_pieces<<" pieces, "<<seeders.size()<<" seeders)"<<endl;
    auto d_ptr = make_shared<DownloadState>();
    DownloadState &d = *d_ptr;
    
    d.filesize = filesize;
    d.total_pieces = num_pieces;
    d.completed = 0;
    d.done = false;
    d.dest_path = dest_path + ".part";
    d.whole_sha1 = whole_sha1;
    d.piece_hashes = piece_hashes;
    d.seeders = seeders;
    d.piece_status.assign(d.total_pieces, 0);
    ofstream outfile(d.dest_path, ios::binary);
    if(!outfile){
        cout<<"Cannot create output file: "<<d.dest_path<<endl;
        return;
    }
    outfile.seekp(filesize-1);
    outfile.write("", 1);
    outfile.close();

    ongoing_downloads[key] = d_ptr;
    int workers = min((int)seeders.size(), 4);
    for(int i=0; i<workers; i++){
        thread(download_worker, gid, filename).detach();
    }
    
    cout<<"Download started for "<<key<<" with "<<workers<<" worker threads"<<endl;
}
void show_downloads(){
    if(ongoing_downloads.empty()){
        cout<<"No active downloads"<<endl;
        return;
    }
    
    for(auto &p:ongoing_downloads){
        auto d_ptr = p.second;
        DownloadState &d = *d_ptr;
        lock_guard<mutex> lock(d.mtx);
        cout<<p.first<<" : ";
        if(d.done){
            cout<<"Completed"<<endl;
        }else{
            double progress = (d.completed * 100.0) / d.total_pieces;
            cout<<"In Progress - "<<d.completed<<"/"<<d.total_pieces
                <<" pieces ("<<fixed<<setprecision(1)<<progress<<"%)"<<endl;
        }
    }
}
void stop_share(const string &gid, const string &filename, string &msg){
    string key = gid + ":" + filename;
    if(local_seeding_files.count(key)){
        local_seeding_files.erase(key);
        msg = "Stopped sharing " + filename + " in group " + gid + "\n";
    }else{
        msg = "Not currently sharing " + filename + " in group " + gid + "\n";
    }
}
