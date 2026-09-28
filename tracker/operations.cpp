#include "operations.h"
#include <algorithm>
#include <mutex>

using namespace std;

mutex mtx;

map<string,string> users;
map<string,Group> groups;

bool create_user(const string &id,const string &pass, string &msg){
    lock_guard<mutex> lock(mtx);
    if(users.count(id)){
        msg = "User already exists\n";
        return false;
    }
    users[id] = pass;
    msg = "User created\n";
    return true;
}

bool login(const string &id, const string &pass, string &msg){
    lock_guard<mutex> lock(mtx);
    if(users.count(id) && users[id]==pass){
        msg = "Login successful\n";
        return true;
    }
    if(users.count(id)){
        msg="Wrong password\n";
    }else{
        msg = "User does not exist\n";
    }
    return false;
}

bool create_group(const string &gid,const string &owner, string &msg){
    lock_guard<mutex> lock(mtx);
    if(groups.count(gid)){
        msg = "Group already exists\n";
        return false;
    }
    Group g;
    g.owner=owner;
    g.members.push_back(owner);
    groups[gid]=g;
    msg="Group created\n";
    return true;
}

bool join_group(const string &gid,const string &user,string &msg){
    lock_guard<mutex> lock(mtx);
    if(!groups.count(gid)){
        msg="Group does not exist\n";
        return false;
    }
    Group &g = groups[gid];
    if(find(g.members.begin(),g.members.end(),user)!=g.members.end()){
        msg = "Already a member\n";
        return false;
    }
    if(find(g.requests.begin(),g.requests.end(),user)!= g.requests.end()){
        msg = "Request already pending\n";
        return false;
    }
    g.requests.push_back(user);
    msg = "Join request submitted\n";
    return true;
}

bool leave_group(const string &gid, const string &user, string &msg){
    lock_guard<mutex> lock(mtx);
    if(!groups.count(gid)){
        msg="Group does not exist\n";
        return false;
    }
    Group &g = groups[gid];
    auto it = find(g.members.begin(), g.members.end(), user);
    if(it == g.members.end()){
        msg = "User not in group\n";
        return false;
    }

    if(g.owner==user){
        g.members.erase(it);
        if(!g.members.empty()){
            g.owner=g.members.front();
            msg="Owner left, new owner is "+ g.owner + "\n";
        }else{
            groups.erase(gid);
            msg = "Owner left. Group deleted as no members are left\n";
            return true;
        }
    }else{
        g.members.erase(it);
        msg = "Left group\n";
    }
    return true;
}

bool list_groups(string &msg){
    lock_guard<mutex> lock(mtx);
    msg.clear();
    for(auto &p : groups) msg+=p.first + "\n";
    return !groups.empty();
}

bool list_requests(const string &gid, const string &user,string &msg){
    lock_guard<mutex> lock(mtx);
    if(!groups.count(gid)){
        msg = "Group not found\n";
        return false;
    }
    Group &g = groups[gid];
    if(g.owner != user){
        msg = "Only owner can view requests\n";
        return false;
    }
    if(g.requests.empty()){
        msg="No pending requests\n";
        return true;
    }
    msg.clear();
    for(auto &r : g.requests) msg += r+"\n";
    return true;
}

bool accept_request(const string &gid,const string &owner, const string &user, string &msg){
    lock_guard<mutex> lock(mtx);
    if(!groups.count(gid)){
        msg = "Group not found\n";
        return false;
    }
    Group &g = groups[gid];
    if(g.owner != owner){
        msg = "Only owner can accept\n";
        return false;
    }
    auto it = find(g.requests.begin(),g.requests.end(),user);
    if(it == g.requests.end()){
        msg = "No such requests\n";
        return false;
    }
    g.members.push_back(user);
    g.requests.erase(it);
    msg = "Request accepted\n";
    return true;
}