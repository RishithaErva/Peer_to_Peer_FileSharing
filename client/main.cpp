#include "tracker_connector.h"
#include "file_utils.h"
#include "download_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

using namespace std;

string current_user_id;
int peer_port = 7001;
string my_ip = "127.0.0.1";

string read_response(int sock) {
    char buffer[65536];
    int n = read(sock, buffer, sizeof(buffer)-1);
    if (n <= 0) return "";
    buffer[n] = '\0';
    return string(buffer);
}

string get_local_ip(){
    char hostbuffer[256];
    struct hostent *host_entry;
    
    if(gethostname(hostbuffer, sizeof(hostbuffer)) == -1){
        return "127.0.0.1";
    }
    
    host_entry = gethostbyname(hostbuffer);
    if(host_entry == NULL){
        return "127.0.0.1";
    }
    
    char *ip = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
    return string(ip);
}

int main(int argc, char* argv[]){
    if(argc != 3){
        cout << "Usage: " << argv[0] << " <IP>:<PORT> tracker_info.txt\n";
        return 0;
    }
    string arg1(argv[1]);
    size_t colon = arg1.find(':');
    if(colon != string::npos){
        my_ip = arg1.substr(0, colon);
        peer_port = stoi(arg1.substr(colon+1));
    }else{
        cout<<"Invalid format. Use IP:PORT"<<endl;
        return 1;
    }
    if(my_ip == "127.0.0.1"){
        my_ip = get_local_ip();
    }

    cout<<"Client starting on "<<my_ip<<":"<<peer_port<<endl;
    char tracker_file[256];
    snprintf(tracker_file, sizeof(tracker_file), "../%s", argv[2]);
    FILE *f = fopen(tracker_file, "r");
    if(!f){
        perror("fopen");
        return 1;
    }
    for(int i=0; i<2; i++){
        char ip[50];
        int cport, sport;
        if(fscanf(f, "%s %d %d", ip, &cport, &sport) == 3){
            trackers[i].ip = string(ip);
            trackers[i].client_port = cport;
            trackers[i].sync_port = sport;
        }
    }
    fclose(f);
    thread(peer_listener_thread, peer_port).detach();
    this_thread::sleep_for(chrono::milliseconds(500)); 
    int sock = -1;
    int tracker_index = -1;
    for(int i=0; i<2; i++){
        sock = connect_to_tracker(trackers[i].ip, trackers[i].client_port);
        if(sock >= 0){
            tracker_index = i;
            break;
        }
    }

    if(sock < 0){
        cout << "No tracker available\n";
        return 1;
    }
    cout << "Connected to tracker " << trackers[tracker_index].ip 
         << ":" << trackers[tracker_index].client_port << endl;

    string cmd;
    while(true){
        cout << "-> ";
        if(!getline(cin, cmd)) continue;
        while(!cmd.empty() && isspace(cmd.front())) cmd.erase(cmd.begin());
        while(!cmd.empty() && isspace(cmd.back())) cmd.pop_back();
        
        if(cmd.empty()) continue;
        
        if(cmd == "quit"){
            cout << "Client exiting...\n";
            break;
        }
        if(cmd.rfind("upload_file ",0) == 0){
            string gid, filepath;
            istringstream ss(cmd);
            string token;
            ss >> token >> gid >> filepath;
            if(gid.empty() || filepath.empty()){
                cout << "Usage: upload_file <group_id> <filepath>\n";
                continue;
            }
            upload_file(sock, gid, filepath, my_ip, peer_port);
            continue;
        }
        if(cmd.rfind("download_file ",0) == 0){
            string gid, filename, dest;
            istringstream ss(cmd);
            string token;
            ss >> token >> gid >> filename >> dest;
            if(gid.empty() || filename.empty() || dest.empty()){
                cout << "Usage: download_file <group_id> <filename> <destination_path>\n";
                continue;
            }
            start_download(sock, gid, filename, dest, my_ip, peer_port);
            continue;
        }
        if(cmd == "show_downloads"){
            show_downloads();
            continue;
        }
        if(cmd.rfind("stop_share ",0) == 0){
            string gid, filename;
            istringstream ss(cmd);
            string token;
            ss >> token >> gid >> filename;
            if(gid.empty() || filename.empty()){
                cout << "Usage: stop_share <group_id> <filename>\n";
                continue;
            }
            string stop_msg = "stop_share " + gid + " " + filename + " " + my_ip + " " + to_string(peer_port) + "\n";
            write(sock, stop_msg.c_str(), stop_msg.size());
            string reply = read_response(sock);
            if(!reply.empty()) cout << reply;

            string msg;
            stop_share(gid, filename, msg);
            cout << msg;
            continue;
        }
        string to_send = cmd + "\n";
        if(write(sock, to_send.c_str(), to_send.size()) < 0){
            close(sock);
            sock = reconnect(tracker_index, tracker_index);
            if(sock < 0){
                cout << "No tracker available. Exiting...\n";
                break;
            }
            if(!current_user_id.empty()){
                string session_msg = "session_restore " + current_user_id + "\n";
                write(sock, session_msg.c_str(), session_msg.size());
                string restore_reply = read_response(sock);
                if(!restore_reply.empty()) cout << restore_reply;
            }
            write(sock, to_send.c_str(), to_send.size());
        }

        string reply = read_response(sock);
        if(!reply.empty()){
            cout << reply;
            if(cmd.rfind("login ",0) == 0 && reply.find("successful") != string::npos){
                istringstream iss(cmd);
                string token, id, pass;
                iss >> token >> id >> pass;
                current_user_id = id;
            }
        } else {
            close(sock);
            sock = reconnect(tracker_index, tracker_index);
            if(sock < 0){
                cout << "No tracker available. Exiting...\n";
                break;
            }
            if(!current_user_id.empty()){
                string session_msg = "session_restore " + current_user_id + "\n";
                write(sock, session_msg.c_str(), session_msg.size());
                string restore_reply = read_response(sock);
                if(!restore_reply.empty()) cout << restore_reply;
            }
        }
    }

    close(sock);
    return 0;
}
