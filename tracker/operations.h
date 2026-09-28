#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <string>
#include<vector>
#include<map>

using namespace std;

struct Group{
    string owner;
    vector<string> members;
    vector<string> requests;
};

extern map<string,string> users;
extern map<string,Group> groups;

bool create_user(const string &id, const string &pass, string &msg);
bool login(const string &id,const string &pass,string &msg);

bool create_group(const string &gid,const string &owner, string &msg);
bool join_group(const string &gid, const string &user,string &msg);
bool leave_group(const string &gid,const string &user, string &msg);
bool list_groups(string &msg);
bool list_requests(const string &gid,const string &user, string &msg);
bool accept_request(const string &gid, const string &owner,const string &user, string &msg);

#endif