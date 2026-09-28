#include "tracker_server.h"
#include "operations.h"
#include "file_operations.h"
#include <unistd.h>
#include <arpa/inet.h>
#include<cstring>
#include<cstdlib>
#include<string>
#include<sstream>
#include<cstdio>
#include<iostream>
#include <map>
#include<mutex>
#include<algorithm>

using namespace std;

map<string ,bool> logged_in_users;
map<int,string> client_current_user;

mutex session_mtx;

int tracker_running = 1;
int tracker_no = 0;
char tracker_ips[2][50] = {};
int client_ports[2]={};
int sync_ports[2] = {};

bool list_files(const string &groupname, string &msg){
    lock_guard<mutex> lock(group_files_mtx);
    msg.clear();
    if(group_files.find(groupname)==group_files.end()){
        msg = "Group not found or no files\n";
        return false;
    }
    if(group_files[groupname].empty()){
        msg = "No files in group\n";
        return false;
    }
    for(auto &file_pair : group_files[groupname]){
        msg+= file_pair.second.filename + "\n";
    }
    return true;
}

void *client_handler(void* arg){
    int sock=*(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    string current_user;
    {
        lock_guard<mutex> lock(session_mtx);
        if(client_current_user.count(sock)){
            current_user = client_current_user[sock];
        }
    }

    while(tracker_running){
        int n=read(sock,buffer,BUFFER_SIZE-1);
        if(n<=0) break;
        buffer[n] = '\0';
        string cmd(buffer),msg;

        while (!cmd.empty() && isspace(cmd.front())) cmd.erase(cmd.begin());
        while (!cmd.empty() && isspace(cmd.back())) cmd.pop_back();

        if(cmd.rfind("session_restore ",0)==0){
            char id[50];
            if(sscanf(cmd.c_str(), "session_restore %s",id)==1){
                current_user = id;
                {
                    lock_guard<mutex> lock(session_mtx);
                    logged_in_users[id] = true;
                    client_current_user[sock]=id;
                }
                msg = "Session restored for user "+ current_user + "\n"; 
            }else{
                msg = "Error: Invalid session restore\n";
            }
            write(sock,msg.c_str(),msg.size());
            continue;
        }


        if(cmd.rfind("create_user ",0)==0){
            char id[50],pass[50];
            if(sscanf(buffer+12,"%s %s",id,pass)==2){
                create_user(string(id),string(pass),msg);
                send_sync(buffer);
            }else msg = "Usage : create_user <id> <password>\n";
        }


        else if(cmd.rfind("login ",0)==0){
            char id[50],pass[50];
            if(sscanf(cmd.c_str(),"login %s %s",id,pass)!=2){
                msg = "usage: login <id> <password>\n";
            }else{
                if(login(id,pass,msg)){
                    current_user=id;
                    {
                        lock_guard<mutex> lock(session_mtx);
                        logged_in_users[id] = true;
                        client_current_user[sock]=id;
                    }
                    char syncmsg[100];
                    snprintf(syncmsg,sizeof(syncmsg),"login_sync %s",id);
                    send_sync(syncmsg);
                }
            }
        }


        else if(cmd.rfind("create_group ",0)==0){
            char gid[50];
            if(current_user.empty()){
                msg = "Error: No user logged in\n";
            }else if(sscanf(cmd.c_str(),"create_group %s",gid)!=1){
                msg = "Usage: create_group <gid>\n";
            }else{
                create_group(gid,current_user,msg);
                char syncmsg[200];
                snprintf(syncmsg,sizeof(syncmsg),"create_group %s %s",gid,current_user.c_str());
                send_sync(syncmsg);
            }
        }


        else if(cmd.rfind("join_group ",0)==0){
            char gid[50];
            if(current_user.empty()){
                msg = "Error: No user logged in\n";
            }else if(sscanf(cmd.c_str(),"join_group %s",gid)!=1){
                msg = "Usage: join_group <gid>\n";
            }else{
                join_group(gid,current_user,msg);
                char syncmsg[200];
                snprintf(syncmsg,sizeof(syncmsg),"join_group %s %s",gid,current_user.c_str());
                send_sync(syncmsg);
            }
        }


        else if(cmd.rfind("list_requests ",0)==0){
            char gid[50];
            if(current_user.empty()){
                msg = "Error: No user logged in\n";
            }else if(sscanf(buffer+14, "%s",gid)==1){
                list_requests(gid,current_user,msg);
            }else{
                msg = "Usage: list_requests <gid>\n";
            }
        }

        else if (cmd.rfind("leave_group ", 0) == 0) {
            char gid[50];
            if(current_user.empty()){
                msg="Error: No user logged in\n";
            }else if(sscanf(cmd.c_str(),"leave_group %s",gid)!=1){
                msg = "Usage : leave_group <gid>\n";
            }else{
                leave_group(gid, current_user, msg);
                char syncmsg[200];
                snprintf(syncmsg,sizeof(syncmsg),"leave_group %s %s",gid,current_user.c_str());
                send_sync(syncmsg);
            }
        }

        else if(cmd.rfind("accept_request ",0)==0){
            char gid[50],uid[50];
            if(current_user.empty()){
                msg = "Error: No user logged in\n";
            }else if(sscanf(cmd.c_str(),"accept_request %s %s",gid,uid)!=2){
                msg = "Usage: accept_request <gid> <user>\n";
            }else{
                accept_request(gid,current_user,uid,msg);
                char syncmsg[200];
                snprintf(syncmsg,sizeof(syncmsg),"accept_request %s %s",gid,current_user.c_str());
                send_sync(syncmsg);
            }
        }

        else if(cmd == "list_groups"){
            if(!list_groups(msg)){
                msg = "No groups available\n";
            }
        }

        else if(cmd.rfind("upload_file",0)==0){
            if(current_user.empty()){
                msg = "Error: No user logged in\n";
            }else{
                stringstream ss(cmd);
                string token,gid,filename,seeder_ip;
                uint64_t filesize;
                string file_sha1;
                int num_pieces, seeder_port;

                ss>> token>> gid>> filename>> filesize>> file_sha1>> num_pieces>>seeder_ip>>seeder_port;
                if(gid.empty() || filename.empty() || num_pieces <= 0){
                    msg = "Error: Invalid upload parameters\n";
                    write(sock, msg.c_str(), msg.size());
                    continue;
                }

                vector<string> piece_hashes;
                for(int i=0; i<num_pieces; i++){
                    string hash;
                    ss >> hash;
                    if(!hash.empty()) piece_hashes.push_back(hash);
                }
                if((int)piece_hashes.size() != num_pieces){
                    msg = "Error: Piece count mismatch\n";
                    write(sock, msg.c_str(), msg.size());
                    continue;
                }
                {
                    lock_guard<mutex> lock(session_mtx);
                    if(groups.find(gid)==groups.end() || 
                       find(groups[gid].members.begin(), groups[gid].members.end(),current_user) == groups[gid].members.end()) {
                        msg = "Error: You must be a member of group " + gid + " to upload files\n";
                        write(sock, msg.c_str(), msg.size());
                        continue;
                    }
                }
                handle_upload_file(gid, filename, current_user, filesize, file_sha1, 
                                 piece_hashes, seeder_ip, seeder_port, msg);

                stringstream syncss;
                syncss << "upload_file " << gid << " " << filename << " " << filesize 
                       << " " << file_sha1 << " " << num_pieces << " " 
                       << seeder_ip << " " << seeder_port << " ";
                for(auto &ph : piece_hashes) syncss << ph << " ";
                send_sync(syncss.str().c_str());
            }
        }

        else if(cmd.rfind("download_file ", 0) == 0){
            stringstream ss(cmd);
            string token, gid, filename, client_ip;
            int client_port = 0;
            ss >> token >> gid >> filename >> client_ip >> client_port;
            if(gid.empty() || filename.empty()){
                msg = "Usage: download_file <group> <filename> <client_ip> <client_port>\n";
                write(sock, msg.c_str(), msg.size());
                continue;
            }
            stringstream response;
            lock_guard<mutex> lock(group_files_mtx);

            if(group_files.find(gid) != group_files.end() && group_files[gid].count(filename)){
                FileInfo &fi = group_files[gid][filename];
                if(!client_ip.empty() && client_port > 0){
                }
                
                if(fi.seeders.empty()){
                    response << "No seeders available for this file\n";
                } else {
                    response << fi.filesize << " " << fi.file_sha1 << " " << fi.piece_sha1.size() << "\n";
                    for(auto &ph : fi.piece_sha1) response << ph << "\n";
                    for(auto &s : fi.seeders) {
                        response << s.ip << ":" << s.port << " ";
                    }
                    response << "\n";
                }
            } else {
                response << "File not found in group\n";
            }
            string resp_str = response.str();
            write(sock, resp_str.c_str(), resp_str.size());
            continue;
        }


        else if(cmd.rfind("stop_share ", 0) == 0){
            stringstream ss(cmd);
            string token, gid, filename, ip;
            int port;
            ss >> token >> gid >> filename >> ip >> port;   
            if(gid.empty() || filename.empty() || ip.empty() || port <= 0){
                msg = "Error: Invalid stop_share parameters\n";
            } else {
                lock_guard<mutex> lock(group_files_mtx);
                
                if(group_files.find(gid) != group_files.end() && 
                   group_files[gid].count(filename)){ 
                    FileInfo &fi = group_files[gid][filename];
                    SeederInfo seeder_to_remove = {ip, port};   
                    auto it = fi.seeders.find(seeder_to_remove);
                    if(it != fi.seeders.end()){
                        fi.seeders.erase(it);
                        msg = "Removed as seeder for " + filename + "\n";
                        stringstream syncss;
                        syncss << "stop_share " << gid << " " << filename << " " << ip << " " << port;
                        send_sync(syncss.str().c_str());
                    } else {
                        msg = "You were not listed as a seeder for this file\n";
                    }
                } else {
                    msg = "File not found in group\n";
                }
            }
        }


        else if(cmd.rfind("add_seeder ", 0) == 0){
            stringstream ss(cmd);
            string token, gid, filename, ip;
            int port;
            ss >> token >> gid >> filename >> ip >> port;
            if(!gid.empty() && !filename.empty() && !ip.empty() && port > 0){
                lock_guard<mutex> lock(group_files_mtx);
                if(group_files.find(gid) != group_files.end() && 
                   group_files[gid].count(filename)){
                    FileInfo &fi = group_files[gid][filename];
                    fi.seeders.insert({ip, port});
                    msg = "Added as seeder for " + filename + "\n";
                    stringstream syncss;
                    syncss << "add_seeder " << gid << " " << filename << " " << ip << " " << port;
                    send_sync(syncss.str().c_str());
                } else {
                    msg = "File not found\n";
                }
            } else {
                msg = "Error: Invalid add_seeder parameters\n";
            }
        }


        else if(cmd.rfind("list_files ",0)==0){
            char gid[50];
            if(sscanf(cmd.c_str(), "list_files %s",gid)==1){
                if(!list_files(gid,msg)) msg = "No files found in the group\n";
            }else{
                msg = "Usage: list_files <group_name>\n";
            }
        }

        else if(cmd.rfind("logout")==0){
            if(current_user.empty()) msg="No user logged in\n";
            else{
                char syncmsg[100];
                snprintf(syncmsg,sizeof(syncmsg),"logout_sync %s",current_user.c_str());
                send_sync(syncmsg);
                {
                    lock_guard<mutex> lock(session_mtx);
                    logged_in_users[current_user] = false;
                }
        
                msg = "Logout successful\n";
                current_user.clear();
            }
        }

        else msg = "Unknown command\n";
        if(!msg.empty()) write(sock,msg.c_str(),msg.size());
    }
    {
        lock_guard<mutex> lock(session_mtx);
        client_current_user.erase(sock);
    }
    close(sock);
    return NULL;
}



void* tracker_sync_listener(void *arg){
    int port = *(int *)arg;
    free(arg);

    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd<0){
        perror("socket");
        exit(1);
    }
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr{},cli_addr{};
    socklen_t cli_len =sizeof(cli_addr);
    char buffer[BUFFER_SIZE];

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if(bind(sockfd,(struct sockaddr*) &addr,sizeof(addr))<0){
        perror("bind");
        exit(1);
    }
    listen(sockfd,5);
    printf("Tracker sync listening on port %d\n",port);

    while(tracker_running){
        int new_sock = accept(sockfd,(struct sockaddr*)&cli_addr, &cli_len);
        if(new_sock<0){
            usleep(100000);
            continue;
        }
        int n=read(new_sock,buffer,BUFFER_SIZE-1);
        if(n>0){
            buffer[n]='\0';
            string msg;

            if(strncmp(buffer,"create_user ",12)==0){
                char id[50],pass[50];
                if(sscanf(buffer+12, "%s %s",id,pass)==2)
                    create_user(string(id),string(pass),msg);
            }
            else if(strncmp(buffer,"create_group ",13)==0){
                char gid[50],owner[50];
                if(sscanf(buffer+13, "%s %s",gid,owner)==2)
                    create_group(string(gid),string(owner),msg);
            }
            else if(strncmp(buffer,"join_group ",11)==0){
                char gid[50],user[50];
                if(sscanf(buffer+11, "%s %s",gid,user)==2)
                    join_group(string(gid),string(user),msg);
            }
            else if(strncmp(buffer,"leave_group ",12)==0){
                char gid[50],user[50];
                if(sscanf(buffer+12, "%s %s",gid,user)==2){
                    leave_group(string(gid),string(user),msg);
                }else{
                    msg = "Usage: leave_group <gid>\n";
                }
            }
            else if (strncmp(buffer,"accept_request ",15)==0) {
                char gid[50],owner[50],user[50];
                if (sscanf(buffer +15, "%s %s %s", gid,owner,user)==3)
                    accept_request(string(gid),string(owner),string(user), msg);
            }
            else if(strncmp(buffer,"login_sync ",11)==0){
                char id[50];
                if(sscanf(buffer+11,"%s",id)==1){
                    logged_in_users[id] = true;
                }
            }
            else if(strncmp(buffer, "upload_file ",12)==0){
                stringstream ss(buffer);
                string token, gid, filename, file_sha1, seeder_ip;
                uint64_t filesize;
                int num_pieces, seeder_port;
                
                ss >> token >> gid >> filename >> filesize >> file_sha1 >> num_pieces 
                   >> seeder_ip >> seeder_port;

                vector<string> piece_hashes;
                for(int i=0; i<num_pieces; i++){
                    string hash;
                    ss >> hash;
                    if(!hash.empty()) piece_hashes.push_back(hash);
                }

                string dummy_msg;
                handle_upload_file(gid, filename, "", filesize, file_sha1, piece_hashes, seeder_ip, seeder_port, dummy_msg);
            }

            else if(strncmp(buffer, "stop_share ", 11)==0){
                stringstream ss(buffer);
                string token, gid, filename, ip;
                int port;
                ss >> token >> gid >> filename >> ip >> port;
                
                if(!gid.empty() && !filename.empty() && !ip.empty() && port > 0){
                    lock_guard<mutex> lock(group_files_mtx);
                    if(group_files.find(gid) != group_files.end() && 
                       group_files[gid].count(filename)){
                        SeederInfo seeder_to_remove = {ip, port};
                        group_files[gid][filename].seeders.erase(seeder_to_remove);
                    }
                }
            }
            else if(strncmp(buffer, "add_seeder ", 11)==0){
                stringstream ss(buffer);
                string token, gid, filename, ip;
                int port;
                ss >> token >> gid >> filename >> ip >> port;
                
                if(!gid.empty() && !filename.empty() && !ip.empty() && port > 0){
                    lock_guard<mutex> lock(group_files_mtx);
                    if(group_files.find(gid) != group_files.end() && 
                       group_files[gid].count(filename)){
                        group_files[gid][filename].seeders.insert({ip, port});
                    }
                }
            }
            else if(strncmp(buffer,"logout_sync ",12)==0){
                char id[50];
                if(sscanf(buffer+12,"%s",id)==1){
                    lock_guard<mutex> lock(session_mtx);
                    logged_in_users[id] = false;
                }
            }
        }

        close(new_sock);
    }
    close(sockfd);
    return nullptr;
}


void send_sync(const char* msg){
    int other = (tracker_no == 1) ? 1 : 0;
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd<0) return;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(sync_ports[other]);
    inet_pton(AF_INET,tracker_ips[other],&addr.sin_addr);
    if(connect(sockfd,(struct sockaddr*)&addr,sizeof(addr))<0){
        close(sockfd);
        return;
    }

    write(sockfd,msg,strlen(msg));
    close(sockfd);
}