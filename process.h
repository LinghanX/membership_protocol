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
#include <chrono>

enum process_state {
    LEADER,
    MEMBER,
    NON_MEMBER,
    Waiting_ACK,
    DELETING,
    JOINING
};
struct member_stat {
    int id;
    bool alive;
    std::string address;
    bool acknowledge;
    std::chrono::time_point<std::chrono::high_resolution_clock> last_heartbeat_received;
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
    process_state curr_state;
    std::vector<member_stat> members;
    int view_id;
    std::string udp_port;
    int pending_operation; // 0 is add, 1 is delete

    int pending_member_id;
    int leader_id;
    static msg_type check_msg_type(void* msg, ssize_t size);
    static bool all_member_ack(Process *);
    static void bring_proc_online(char* proc_id, Process*);
    static void get_member_list(char*, Process* proc);
    static void * start_leader(void *);
    static void * start_member(void *);
    static void * start_udp_listen(void *);
    static void * start_udp_send(void *);
    static void send_msg(std::string, Process*);
    static int recv_msg(std::string, Process*);
    static int any_mem_offline(Process* self);
    Process(std::vector<std::string> &, std::string);
    void init();
};

#endif //PRJ2_PROCESS_H
