//
// Created by Linghan Xing on 10/27/18.
//

#ifndef PRJ2_MESSAGE_H
#define PRJ2_MESSAGE_H

#include <arpa/inet.h>

enum OPERATION {
    ADD,
    DEL,
    PENDING
};

typedef struct {
    uint32_t type;
    uint32_t view_id;
    uint32_t operation;
    uint32_t peer_id;
} Req_Msg;

typedef struct  {
    uint32_t type;
    uint32_t peer_id;
} OK_Msg;

typedef struct {
    uint32_t type;
    uint32_t view_id;
    uint32_t new_proc_id;
    char member_list[16];
} new_view_msg ;

typedef struct {
    uint32_t type;
} new_leader_msg ;

typedef struct {
    uint32_t type;
    uint32_t proc_id;
} join_msg ;

typedef struct {
    uint32_t type;
    uint32_t proc_id;
} heartbeat_msg;

new_view_msg* hton(new_view_msg* msg);
new_view_msg* ntoh(new_view_msg* msg);
OK_Msg* hton(OK_Msg* msg);
OK_Msg* ntoh(OK_Msg* msg);
Req_Msg* hton(Req_Msg* msg);
Req_Msg* ntoh(Req_Msg* msg);
join_msg* hton(join_msg* msg);
join_msg* ntoh(join_msg* msg);


#endif //PRJ2_MESSAGE_H
