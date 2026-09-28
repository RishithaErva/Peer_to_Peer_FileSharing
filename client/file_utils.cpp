#include "file_utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/evp.h>
#include <vector>
#include <cstring>
#include<unistd.h>
#include<string>
#include<arpa/inet.h>
#include<thread>

#define BUFFER_SIZE 1024

using namespace std;

int PIECE_SIZE = 512*1024;
map<string,pair<string,uint64_t>> local_seeding_files;
map<string,shared_ptr<DownloadState>> ongoing_downloads;

static string to_hex(const unsigned char* digest, size_t len){
    stringstream ss;
    ss<<hex<<setfill('0');
    for(size_t i=0;i<len;i++){
        ss<<setw(2)<<(int)digest[i];
    }
    return ss.str();
}

string sha1_file(const string& path){
    ifstream f(path,ios::binary);
    if(!f) return "";
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx,EVP_sha1(),nullptr);
    char buf[8192];
    while(f){
        f.read(buf,sizeof(buf));
        streamsize n = f.gcount();
        if(n>0) EVP_DigestUpdate(ctx,buf,n);
    }
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    EVP_DigestFinal_ex(ctx,hash,&len);
    EVP_MD_CTX_free(ctx);
    return to_hex(hash,len);
}

vector<string> piece_hashes(const string& path){
    ifstream f(path, ios::binary);
    vector<string> res;
    if(!f) return res;
    vector<char> buf(PIECE_SIZE);
    while(f){
        f.read(buf.data(),PIECE_SIZE);
        streamsize n = f.gcount();
        if(n<=0) break;
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx,EVP_sha1(),nullptr);
        EVP_DigestUpdate(ctx,buf.data(),n);
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int len=0;
        EVP_DigestFinal_ex(ctx,hash,&len);
        EVP_MD_CTX_free(ctx);

        res.push_back(to_hex(hash,len));
    }
    return res;
}


void peer_listener_thread(int peer_port){
    int sockfd =socket(AF_INET,SOCK_STREAM,0);
    if(sockfd <0){
        perror("socket");
        return;
    }
    int opt=1;
    setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    
    sockaddr_in addr{};
    addr.sin_family =AF_INET;
    addr.sin_addr.s_addr =INADDR_ANY;
    addr.sin_port =htons(peer_port);
    if(bind(sockfd,(sockaddr*)&addr,sizeof(addr))<0){ 
        perror("bind"); 
        close(sockfd);
        return; 
    }
    listen(sockfd,5);
    cout<<"Peer listening on port "<<peer_port<<endl;
    while(true){
        sockaddr_in cli{};
        socklen_t len=sizeof(cli);
        int client_fd=accept(sockfd,(sockaddr*)&cli,&len);
        if(client_fd>=0){
            thread t(handle_peer_connection,client_fd);
            t.detach();
        }
    }
}

void handle_peer_connection(int client_fd){
    char buf[BUFFER_SIZE];
    int n = read(client_fd,buf,BUFFER_SIZE-1);
    if(n <=0){
        close(client_fd);
        return;
    }
    buf[n]='\0';
    stringstream ss(buf);
    string cmd,gid,filename;
    int idx;
    ss>>cmd>>gid>>filename>>idx;
    string key =gid+":"+filename;
    if(local_seeding_files.count(key)==0){ 
        close(client_fd); 
        return; 
    }
    
    string path=local_seeding_files[key].first;
    ifstream f(path,ios::binary);
    if(!f){
        close(client_fd);
        return;
    }
    f.seekg((long long)idx*PIECE_SIZE);
    vector<char> piece(PIECE_SIZE);
    f.read(piece.data(),PIECE_SIZE);
    streamsize read_bytes = f.gcount();
    
    write(client_fd,&read_bytes,sizeof(int));
    if(read_bytes>0){
        write(client_fd,piece.data(),read_bytes);
    }
    close(client_fd);
}


bool download_piece_from_seeder(const string &peer_ip,int peer_port,const string &gid,const string &filename,int idx,vector<char> &out_data){
    int sock = socket(AF_INET,SOCK_STREAM,0);
    if(sock<0) return false;
    sockaddr_in addr{};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(peer_port);
    inet_pton(AF_INET,peer_ip.c_str(),&addr.sin_addr);
    if(connect(sock,(sockaddr*)&addr,sizeof(addr))<0){ 
        close(sock); 
        return false; 
    }
    
    string cmd = "GET_PIECE "+gid+" "+filename+" "+to_string(idx);
    write(sock,cmd.c_str(),cmd.size());
    int piece_size;
    int n = read(sock,&piece_size,sizeof(int));
    if(n!=sizeof(int) || piece_size<=0){ 
        close(sock); 
        return false; 
    }
    out_data.resize(piece_size);
    int total=0;
    while(total<piece_size){
        int r = read(sock,out_data.data()+total, piece_size-total);
        if(r<=0){
            close(sock);
            return false; 
        }
        total+=r;
    }
    close(sock);
    return true;
}



void upload_file(int tracker_sock,const string &gid,const string &filepath,const string &my_ip,int my_port){
    ifstream f(filepath,ios::binary);
    if(!f){
        cout<<"File not found: "<<filepath<<endl;
        return;
    }
    f.seekg(0,ios::end);
    uint64_t filesize=f.tellg();
    f.seekg(0,ios::beg);

    string file_sha1=sha1_file(filepath);
    vector<string> pieces=piece_hashes(filepath);
    size_t pos=filepath.find_last_of("/\\");
    string filename=(pos != string::npos)? filepath.substr(pos+1) :filepath;

    stringstream ss;
    ss << "upload_file " << gid << " " << filename << " " << filesize << " " << file_sha1 << " " << pieces.size() << " " << my_ip << " " << my_port << " ";

    for(auto &p:pieces) ss<<p << " ";
    ss <<"\n";

    string msg=ss.str();
    write(tracker_sock,msg.c_str(),msg.size());

    char buf[1024];
    int n=read(tracker_sock,buf,sizeof(buf)-1);
    if(n>0){ 
        buf[n]='\0'; 
        cout << buf;
        if(string(buf).find("successfully") != string::npos){
            local_seeding_files[gid+":"+filename] = {filepath, filesize};
            cout<<"Now seeding: "<<filename<<" for group "<<gid<<endl;
        }
    }
}
