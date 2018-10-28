//
// Created by Linghan Xing on 10/27/18.
//

#include "message.h"

//typedef struct {
//    int type;
//    int view_id;
//    int new_proc_id;
//} new_view_msg ;
new_view_msg* hton(new_view_msg* msg){
    msg -> type = htonl(msg -> type);
    msg -> view_id = htonl(msg -> view_id);
    msg -> new_proc_id = htonl(msg -> new_proc_id);

    return msg;
}
new_view_msg* ntoh(new_view_msg* msg){
    msg -> view_id = ntohl(msg -> view_id);
    msg -> new_proc_id = ntohl(msg -> new_proc_id);

    return msg;
};
Req_Msg* hton(Req_Msg* msg) {
    msg -> type = htonl(msg -> type);
    msg -> view_id = htonl(msg -> view_id);
    msg -> operation = htonl(msg -> operation);
    msg -> peer_id = htonl(msg -> peer_id);
    return msg;
};
Req_Msg* ntoh(Req_Msg* msg) {
    msg -> view_id = ntohl(msg -> view_id);
    msg -> operation = ntohl(msg -> operation);
    msg -> peer_id = ntohl(msg -> peer_id);
    return msg;
};
OK_Msg* hton(OK_Msg* msg) {
    msg -> type = htonl(msg -> type);
    msg -> peer_id = htonl(msg -> peer_id);
    return msg;
}
OK_Msg* ntoh(OK_Msg* msg) {
    msg -> peer_id = ntohl(msg -> peer_id);
    return msg;
}

join_msg* hton(join_msg* msg) {
    msg -> type = htonl(msg -> type);
    msg -> proc_id = htonl(msg -> proc_id);

    return msg;
};

join_msg* ntoh(join_msg* msg) {
    msg -> proc_id = ntohl(msg -> proc_id);
    return msg;
};
