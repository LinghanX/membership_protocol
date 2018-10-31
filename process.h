//
// Created by Linghan Xing on 10/27/18.
//

#ifndef PRJ2_PROCESS_H
#define PRJ2_PROCESS_H

#include <string>
#include <vector>
#include "message.h"
#include "network.h"
#include <pthread.h>

enum process_state {
    LEADER,
    MEMBER,
    NON_MEMBER,
    Waiting_ACK
};
struct member_stat {
    int id;
    bool alive;
    std::string address;
    bool acknowledge;
};
enum msg_type {
    req,
    ok,
    new_view,
    new_leader,
    join,
    unknown
};

class Process {
protected:

public:
    int my_id;
    std::string port;
    std::vector<std::string> addr_book;
    process_state curr_state;
    std::vector<member_stat> members;
    int view_id;

    int pending_member_id;
    int leader_id;
    void request_membership();
    static void *tcp_member_listen(void *);
    static msg_type check_msg_type(void* msg, ssize_t size);
    void broadcast_heartbeat(Req_Msg *);
    void send_msg(void *msg, std::string addr, ssize_t size);
    static bool all_member_ack(Process *);
    static void bring_proc_online(char* proc_id, Process*);
    static void get_member_list(char*, Process* proc);
    static void * start_leader(void *);
    static void * start_member(void *);
    Process(std::vector<std::string> &, std::string);
    void init();
};

#endif //PRJ2_PROCESS_H
