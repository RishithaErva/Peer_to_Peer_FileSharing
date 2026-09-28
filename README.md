# Assignment-3 (Peer-to-Peer Distributed File Sharing System)

## Overview
This assignment implements **tracker-client system** for user management and group management and peer-to-peer distributed file sharing system with two synchronized trackers and multiple clients. 
- Clients share files in groups and can act as both downloaders and seeders.
- Trackers maintain metadata about users, groups, files, and file piece hashes.
- The system supports concurrent downloads from multiple peers and ensures file integrity using SHA1 hashes.
- Trackers synchronize their state to provide redundancy and high availability.

## Features
- Create and login users  
- Create, join, and leave groups  
- List pending requests for groups (only accessible to group owners)  
- Accept group join requests (only by group owners) 
- Upload a file to a group and list all the files in the group. 
- Download file downloads the required file from multiple peers and show downloads lists all the downloads.
- Automatic synchronization between two tracker servers  
- Client automatically reconnects to available trackers  

## Requirements
- C++17 compatible compiler (g++ recommended)  
- POSIX environment (Linux/macOS)  

## Project Structure
- Tracker folder: tracker_server.cpp, tracker_server.h, operations.cpp, operations.h, main.cpp, file_operations.cpp, file_operations.h, Makefile
- Client folder : main.cpp, tracker_connector.cpp, tracker_connector.h, file_utils.h, file_utils.cpp, Makefile
- tracker_info.txt

## Compilation and Execution
- For tracker:
1. Move to the tracker directory.
2. make
3. ./tracker tracker_info.txt tracker_no

- For client:
1. Move to the client directory.
2. make
3. ./client <IP>:<PORT> tracker info.txt 

## Client Commands
- create_user <id> <password>
- login <id> <password>
- create_group <gid>
- join_group <gid>
- leave_group <gid>
- list_groups
- list_requests <gid>
- accept_request <gid> <user>
- upload_file <gid> <filepath>
- list_files <gid>
- download_file <gid> <filename> <destpath>
- show_downloads
- stop_share <gid> <filename>
- quit - Exit the client

## Architectural Design
- Client-Tracker Model: Clients connect to trackers via TCP.
- Tracker Synchronization: Trackers sync all operations (user/group creation, join/leave requests) using a dedicated sync port.
- Command Handling: Each tracker handles client commands in a dedicated thread.

## Key Algorithms
- User Authentication:
1. On login, validate credentials against tracker database.
2. Return appropriate messages for wrong password or non-existent user.

- Group Management:
1. create_group: Adds owner as first member.
2. join_group: Adds user to request queue.
3. leave_group: Removes user, reassigns ownership if owner leaves.

- Synchronization:
1. send_sync(msg): Sends operation messages to other tracker(s).
2. Trackers apply messages in same order to maintain consistency.

- File Integrity
1. Piece size: 512KB
1. Hashes: SHA1 at both piece and full-file levels
3. Corrupted pieces are re-downloaded from other peers

- Piece Hashing
1. file_utils.cpp calculates SHA1 for the entire file and each 512KB piece.
2. Piece hashes are stored in tracker metadata.

## Data Structures
- Users: map<string, string> mapping user ID to password.
- Groups: map<string, Group>
- struct Group
- struct FileInfo

### Network Protocol Design
- Client ↔ Tracker: TCP connection, text-based commands terminated by \n.Tracker responds with success/error messages.
- Tracker ↔ Tracker: TCP sync messages for all operations.
- Message Handling: Handles partial reads/writes; always terminated by \0 for string safety.
- Error Codes & Status: Errors returned as string messages.


## Assumptions Made
- Only two trackers supported.
- No persistent storage; all state is in-memory.
- Network messages are not encrypted.
- File piece size fixed at 512KB; final piece may be smaller.
- Sequential piece selection for downloads.
- Tracker failure handling: system works as long as at least one tracker is online.

## Implemented Features
- User creation and login
- Group creation, join, leave
- List pending requests
- Accept join requests
- upload files and list files
- download files and show downloads
- Tracker-to-tracker synchronization
- Client auto-reconnect

## Limitations
- Only two trackers supported
- No persistent storage; all data lost on restart
- No encryption/authentication over network

## Testing Procedures
- Multiple Clients: Tested with 2–3 clients concurrently uploading and downloading files.
- Tracker Failures: Tested by shutting down one tracker; clients continue to operate with the other tracker.
- File Sizes: Tested small files (<512KB), medium (~10MB), and large (~1GB).
- Download Integrity: SHA1 verification ensures file integrity; corrupted pieces re-downloaded.
- Synchronization: Verified that all user/group/file operations on one tracker are reflected on the other tracker.

## Tracker Directory
### tracker_server.cpp
- Implements the main tracker server logic.
- Handles multiple client connections using threads (client_handler).
- Processes client commands: create_user, login, create_group, join_group, leave_group, list_groups, list_requests, accept_request, upload_file, list_files, logout.
- Maintains tracker state, logged-in users, group information, and shared file metadata.
- Starts a tracker-to-tracker sync listener (tracker_sync_listener) to propagate updates to the other tracker.

### tracker_server.h
- Header file declaring constants (BUFFER_SIZE), global variables (tracker_running, tracker_no, tracker_ips, etc.), and function prototypes like client_handler() and tracker_sync_listener().

### operations.cpp
- Implements core operations for users and groups:
        - create_user / login
        - create_group / join_group / leave_group
        - list_groups / list_requests / accept_request
- All logic is centralized here so that both the client handler and tracker synchronization can reuse it.
- Uses a mutex to ensure thread-safety on shared structures

### operations.h
- Header file declaring all functions and data structures (Group, users, groups).

### file_operations.cpp
- Handles file metadata operations and seeding information.
- handle_upload_file() – Stores file info, piece hashes, SHA1, owner (seeder), and group association.
- Uses a thread-safe approach with mutex to protect group_files


### Makefile
- Compiles all tracker source files into the tracker_server executable.
- Includes compilation flags like -std=c++17, -Wall, and -pthread.


## Client Directory
### main.cpp
- Implements the client application.
- Reads tracker info from tracker_info.txt and attempts connection to available tracker.
- Provides command-line interface to read user commands (create_user, login, create_group, join_group, upload_file, list_files, etc.).
- Handles tracker failures and reconnects to another tracker automatically.
- Maintains local session info (current_user_id) and restores session if disconnected.

### tracker_connector.cpp
- Handles network connections to trackers.
- Implements connect_to_tracker() and reconnect() functions to manage TCP connections.

### tracker_connector.h
- Declares the TrackerInfo struct:
- Declares external tracker array (extern TrackerInfo trackers[2];) and network functions.

### file_utils.cpp
- Implements file-related utilities for the client:
1. sha1_file() – Computes SHA1 hash of a file.
2. piece_hashes() – Splits files into 512KB pieces and computes SHA1 for each piece.
3. upload_file() – Uploads file info to tracker (metadata only).
4. file_exists() – Checks if file already exists in group.
5. read_response() – Reads responses from tracker sockets.
- Uses OpenSSL’s EVP library for hashing.

### Makefile
- Compiles client files into the executable client.
- Includes -std=c++17, -Wall, and -pthread flags.


## Summary of Flow
- Start trackers → they listen for clients and sync with each other.
- Start client → reads tracker info → connects to available tracker.
- Client sends commands → tracker executes using operations.cpp.
- Tracker syncs commands to other trackers to maintain consistent state.