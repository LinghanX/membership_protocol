#include <utility>

//
// Created by Linghan Xing on 10/27/18.
//

#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "process.h"
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>

#define MAX_HOSTNAME_LEN 256

Process::Process(std::vector<std::string> &addr_book, std::string port) {
    auto logger = spdlog::get("console");
    char currHostName[MAX_HOSTNAME_LEN]; // my name
    if (gethostname(currHostName, MAX_HOSTNAME_LEN) < 0) {
        logger -> error("not able to get my host name");
    }

    int id = 0;
    bool find_id = false;
    for (auto const& addr : addr_book) {
        member_stat curr_member;

        curr_member.address = addr;
        curr_member.alive = false;
        curr_member.id = id;
        curr_member.acknowledge = false;

        if (addr.compare(currHostName) == 0) {
            find_id = true;
            this->my_id = id;
            curr_member.alive = true;
        }
        id++;
        this -> members.push_back(curr_member);
    }

    if (!find_id) logger -> error("unable to parse my id");

    this -> port = std::move(port);
    this -> view_id = 0;
    this -> addr_book = addr_book;
    this -> pending_member_id = -1;
    this -> leader_id = 0;
    this -> curr_state = this -> my_id == 0
            ? process_state::LEADER : process_state::MEMBER;

    logger -> info("my id is: {}", this -> my_id);
    logger -> info("my leader is: {}", this -> leader_id);
    logger -> info("my state is: {}", this -> curr_state);

//    logger -> info("finished processing args");
//    logger -> info("I am a {}", this -> curr_state);
//
//    for (const auto& n : this -> members) {
//        logger -> info("member {} is: {}, addr is: {}", n.id, n.alive, n.address);
//    }
    init();
}
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
void Process::request_membership() {
    join_msg msg;
    msg.type = 4;
    msg.proc_id = this -> my_id;

    join_msg* msg_to_send = hton(&msg);
    send_msg(msg_to_send, this -> members[0].address, sizeof(join_msg));
    this -> curr_state = process_state::MEMBER;
}
void Process::start_leader() {
    auto logger = spdlog::get("console");
    fd_set master;
    fd_set read_fds;
    int fdmax;

    int listener;
    int newfd;
    struct sockaddr_storage remoteaddr;
    socklen_t addrlen;

    char buf[1024];
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes = 1;
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ( (rv = getaddrinfo(NULL, this -> port.c_str(), &hints, &ai)) != 0 )
        logger -> error("unable to get addr");

    for (p = ai; p != NULL; p = p -> ai_next) {
        listener = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol);

        if (listener < 0) continue;

        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if ( bind(listener, p -> ai_addr, p -> ai_addrlen) < 0 ) {
            close (listener);
            continue;
        }
        break;
    }

    if (p == NULL) logger -> error("unable to bind");

    freeaddrinfo(ai);

    if ( listen(listener, 10) == -1 ) logger -> error("listen error");

    FD_SET(listener, &master);

    fdmax = listener;

    while (true) {
        logger -> info("listening, view is: {}", this -> view_id);
        for (const auto &n : this -> members) {
            if (n.alive) logger -> info("{} is alive", n.address);
        }

        read_fds = master;
        if ( select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1 )
            logger -> error("unable to select");

        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == listener) { // new connection request coming
                    addrlen = sizeof (remoteaddr);
                    newfd = accept(listener, (struct sockaddr *) &remoteaddr, &addrlen);

                    if (newfd == -1) logger -> error("accept error");
                    else {
                        FD_SET(newfd, &master);
                        if (newfd > fdmax) {
                            fdmax = newfd;
                        }
                        printf("select server: new connection from %s on socket %d\n",
                               inet_ntop(remoteaddr.ss_family,
                                         get_in_addr((struct sockaddr*) &remoteaddr),
                                         remoteIP, INET6_ADDRSTRLEN),
                               newfd);
                    }
                } else {
                    if ( (nbytes = recv(i, buf, sizeof(buf), 0)) <= 0) {
                        if (nbytes == 0) {
                            printf("select server: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i);
                        FD_CLR(i, &master);
                    } else {
                        // got message from a client
                        msg_type type = check_msg_type(buf, nbytes);
                        switch (type) {
                            case msg_type::join:
                            {
                                join_msg* recved_msg = ntoh((join_msg *) buf);
                                Req_Msg req;
                                req.type = 0;
                                req.view_id = this -> view_id;
                                req.peer_id = recved_msg -> proc_id;
                                req.operation = 0;

                                this -> pending_member_id = recved_msg -> proc_id;
                                this -> members[this -> my_id].acknowledge = true;

                                if (all_member_ack()) {
                                    this -> curr_state = process_state::LEADER;
                                    this -> view_id += 1;

                                    this -> members[this -> pending_member_id].alive = true;

                                    new_view_msg update_view_msg;
                                    update_view_msg.view_id = this -> view_id;
                                    update_view_msg.type = 2;
                                    update_view_msg.new_proc_id = this -> pending_member_id;
                                    get_member_list(update_view_msg.member_list);

                                    logger -> info("sending view msg: {}", update_view_msg.type);
                                    logger -> info("sending view msg: {}", update_view_msg.new_proc_id);
                                    logger -> info("sending view msg: {}", update_view_msg.view_id);

                                    bring_proc_online(update_view_msg.member_list);

                                    this -> pending_member_id = -1; // reset pending member id;

                                    new_view_msg* packged_msg = hton(&update_view_msg);

                                    for (j = 0; j <= fdmax; j++) {
                                        if (FD_ISSET(j, &master)) {
                                            if (j != listener) {
                                                logger -> info("sending new view message");
                                                logger -> info("size is: {}", sizeof(update_view_msg));
                                                if (send(j, packged_msg, sizeof(new_view_msg), 0) == -1)
                                                    logger -> error("error sending new view msg");
                                            }
                                        }
                                    }
                                } else {
                                    Req_Msg* packaged = hton(&req);

                                    for (j = 0; j <= fdmax; j++) {
                                        if (FD_ISSET(j, &master)) {
                                            // we dont want to send to ourself and the process that
                                            // requested to join
                                            if (j != listener && j != i) {
                                                if (send(j, packaged, sizeof(Req_Msg), 0) == -1)
                                                    logger -> error("error sending");
                                            }
                                        }
                                    }
                                    this -> curr_state = process_state::Waiting_ACK;
                                }
                                break;
                            }
                            case msg_type::ok:
                            {
                                OK_Msg* recved_msg = ntoh((OK_Msg *) buf);
                                int peer_id = recved_msg->peer_id;
                                this -> members[peer_id].acknowledge = true;

                                if (all_member_ack()) {
                                    this -> curr_state = process_state::LEADER;
                                    this -> view_id += 1;

                                    this -> members[this -> pending_member_id].alive = true;

                                    new_view_msg update_view_msg;
                                    update_view_msg.view_id = this -> view_id;
                                    update_view_msg.type = 2;
                                    update_view_msg.new_proc_id = this -> pending_member_id;
                                    get_member_list(update_view_msg.member_list);

                                    this -> pending_member_id = -1; // reset pending member id;
                                    bring_proc_online(update_view_msg.member_list);

                                    new_view_msg* packged_msg = hton(&update_view_msg);

                                    for (j = 0; j <= fdmax; j++) {
                                        if (FD_ISSET(j, &master)) {
                                            if (j != listener) {
                                                if (send(j, packged_msg, sizeof(new_view_msg), 0) == -1)
                                                    logger -> error("error sending new view msg");
                                            }
                                        }
                                    }
                                }

                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}
void Process::get_member_list(char* arr) {
    for (int i = 0; i < this -> members.size(); i++) {
        if (this -> members[i].alive) {
            arr[i] = 't';
        } else arr[i] = 'f';
    }
}
void Process::bring_proc_online(char* proc_id) {
    const auto logger = spdlog::get("console");
    logger ->info("view id is: {}", this -> view_id);
    for (int i = 0; i < this -> members.size(); i++) {
        if (proc_id[i] == 't') {
            this -> members[i].alive = true;
            logger ->info("{} is alive", this -> members[i].address);
        } else this -> members[i].alive = false;
    }
}
void Process::handle_message(int size, char* buffer) {
}
void Process::init_new_view() {}
bool Process::all_member_ack() {
    for (const auto &n : this -> members ) {
        if (!n.alive) continue;
        if (!n.acknowledge) return false;
    }
    return true;
}
void Process::broadcast_req_msg(Req_Msg* msg) {
    const auto logger = spdlog::get("console");
    char buffer[sizeof(Req_Msg)];
    memcpy(buffer, msg, sizeof(Req_Msg));

    Req_Msg* converted_msg = hton(msg);
    if (converted_msg != nullptr) {
        for (const auto &n : this -> members) {
            if (n.alive) {
                send_msg(converted_msg, n.address, sizeof(Req_Msg));
            }
        }
    }
};
void Process::send_msg(void *msg, std::string addr, ssize_t size) {
    int sockfd, numbytes;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(addr.c_str(), this -> port.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
              s, sizeof s);
    printf("l 336: client: connecting to %s\n", s);
    freeaddrinfo(servinfo); // all done with this structure

    if ((numbytes = send(sockfd, msg, size, 0)) == -1) {
        perror("recv");
    }
    close(sockfd);
}
msg_type Process::check_msg_type(void *msg, ssize_t size) {
    const auto logger = spdlog::get("console");
    int *first_int = (int *) msg;
    *first_int = ntohl(*first_int);

    logger -> info("msg type is: {}", *first_int);

    if (size == sizeof(join_msg) && *first_int == 4) {
        return msg_type::join;
    } else if (size == sizeof(OK_Msg) && *first_int == 1){
        return msg_type::ok;
    } else if (size == sizeof(Req_Msg) && *first_int == 0) {
        return msg_type::req;
    } else if (size == sizeof(new_view_msg) && *first_int == 2) {
        return msg_type::new_view;
    } else {
        logger -> error("unable to identify incoming message");
        return msg_type::unknown;
    }
}
void Process::start_member() {
    const auto logger = spdlog::get("console");
    int sockfd, numbytes;
    char buf[1024];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    auto leader_addr = this -> members[this -> leader_id].address;
    if ( (rv = getaddrinfo(leader_addr.c_str(), this -> port.c_str(), &hints, &servinfo)) != 0 ) {
        logger -> error("unable to get leader addr");
    }

    for (p = servinfo; p != nullptr; p = p -> ai_next) {
        if ( (sockfd = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1 ) {
            logger -> error("unable to get socket");
            continue;
        }

        if ( connect(sockfd, p -> ai_addr, p -> ai_addrlen) == -1 ) {
            close (sockfd);
            logger -> error("connect error");
            continue;
        }
        break;
    }

    if (p == nullptr) {
        logger -> error("failed to connect");
    }

    inet_ntop(p -> ai_family, get_in_addr((struct sockaddr *) p -> ai_addr), s, sizeof(s));
    printf("client: connecting to %s\n", s);

    join_msg msg;
    msg.type = 4;
    msg.proc_id = this -> my_id;
    this -> members[this -> leader_id].alive = true; // set the leader to be alive

    // ask to join the group
    join_msg* msg_to_send = hton(&msg);
    this -> curr_state = process_state::MEMBER;
    if (send(sockfd, msg_to_send, sizeof(join_msg), 0) == -1)
        logger -> error("error sending new view msg");

    freeaddrinfo(servinfo);

    while (true) {
        logger -> info("listening, view is: {}", this -> view_id);
        for (const auto &n : this -> members) {
            if (n.alive) logger -> info("{} is alive", n.address);
        }
        if ( (numbytes = recv(sockfd, buf, 1024, 0)) <= 0 ) {
            if (numbytes == 0) logger -> error("server hung up");
            else logger -> error("recv error");
        } else {
            logger -> info("receved message {}", numbytes);
            // got message from a client
            msg_type type = check_msg_type(buf, numbytes);
            switch (type) {
                case msg_type::req:
                {
                    OK_Msg ok_msg;
                    ok_msg.type = 1;
                    ok_msg.peer_id = this -> my_id;

                    OK_Msg* packaged_msg = hton(&ok_msg);

                    if (send(sockfd, packaged_msg, sizeof(OK_Msg), 0) == -1)
                        logger -> error("error sending new view msg");

                    break;
                }
                case msg_type ::new_view:
                {
                    new_view_msg* recved_msg = ntoh((new_view_msg *) buf);
                    this -> view_id = recved_msg -> view_id;
                    bring_proc_online(recved_msg -> member_list);

                    break;
                }
            }
        }

        sleep(1);
    }
}
void Process::init() {
    auto logger = spdlog::get("console");
    while (true) {
        switch (this -> curr_state) {
            case process_state::LEADER:
                start_leader();
                break;
            case process_state::NON_MEMBER:
                request_membership();
                break;
            case process_state::MEMBER:
                start_member();
                break;
            case process_state ::Waiting_ACK:
                start_leader();
                break;
            default:
                logger -> error("unknown state");
                break;
        }
    }
}
