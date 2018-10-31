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
        curr_member.last_heartbeat_received = std::chrono::high_resolution_clock::now();

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
    this -> udp_port = "22222";
    this -> pending_operation = -1;
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
int Process::any_mem_offline(Process *self) {
    int id = -1;
    auto curr = std::chrono::high_resolution_clock::now();
    for (const auto &n : self->members) {
        if (n.alive && std::chrono::duration_cast<std::chrono::seconds>(curr - n.last_heartbeat_received).count() > 10) {
            id = n.id;
        }
    }
    return id;
}
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
void *Process::start_leader(void *proc){
    auto self = (Process *) proc;
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
    if ( (rv = getaddrinfo(NULL, self -> port.c_str(), &hints, &ai)) != 0 )
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
        struct timeval tv = {2, 0};

        read_fds = master;
        if ( select(fdmax + 1, &read_fds, NULL, NULL, &tv) == -1 )
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
                                req.view_id = self -> view_id;
                                req.peer_id = recved_msg -> proc_id;
                                req.operation = 0;

                                self -> pending_member_id = recved_msg -> proc_id;
                                self -> pending_operation = 0;
                                self -> members[self -> my_id].acknowledge = true;

                                if (all_member_ack(self)) {
                                    self -> curr_state = process_state::LEADER;
                                    self -> view_id += 1;

                                    self -> members[self -> pending_member_id].alive = true;

                                    new_view_msg update_view_msg;
                                    update_view_msg.view_id = self -> view_id;
                                    update_view_msg.type = 2;
                                    update_view_msg.new_proc_id = self -> pending_member_id;
                                    get_member_list(update_view_msg.member_list, self);

                                    logger -> info("sending view msg: {}", update_view_msg.type);
                                    logger -> info("sending view msg: {}", update_view_msg.new_proc_id);
                                    logger -> info("sending view msg: {}", update_view_msg.view_id);

                                    bring_proc_online(update_view_msg.member_list, self);

                                    self -> pending_member_id = -1; // reset pending member id;

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
                                    self -> curr_state = process_state::Waiting_ACK;
                                }
                                break;
                            }
                            case msg_type::ok:
                            {
                                OK_Msg* recved_msg = ntoh((OK_Msg *) buf);
                                int peer_id = recved_msg->peer_id;
                                self -> members[peer_id].acknowledge = true;

                                if (all_member_ack(self)) {
                                    self -> view_id += 1;
                                    new_view_msg update_view_msg;
                                    update_view_msg.view_id = self -> view_id;
                                    update_view_msg.type = 2;

                                    if (self -> pending_operation == 0) {
                                        self -> members[self -> pending_member_id].alive = true;
                                        self -> pending_operation = -1;
                                    }
                                    else {
                                        self -> members[self -> pending_member_id].alive = false;
                                        self -> pending_operation = -1;
                                    }

                                    update_view_msg.new_proc_id = self -> pending_member_id;
                                    get_member_list(update_view_msg.member_list, self);
                                    bring_proc_online(update_view_msg.member_list, self);
                                    self -> pending_member_id = -1;

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
        int offline_mem_id = any_mem_offline(self);

        if (offline_mem_id >= 0 && offline_mem_id != self -> my_id && self -> pending_operation != 1) {
            logger -> info("requesting removing process {} from the group", offline_mem_id);

            Req_Msg req;
            req.type = 0;
            req.view_id = self -> view_id;
            req.peer_id = offline_mem_id;
            req.operation = 1;

            self -> pending_member_id = offline_mem_id;
            self -> pending_operation = 1;

            self -> members[self -> my_id].acknowledge = true;

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
        }
    }
}
void Process::get_member_list(char* arr, Process* self) {
    for (int i = 0; i < self -> members.size(); i++) {
        if (self -> members[i].alive) {
            arr[i] = 't';
        } else arr[i] = 'f';
    }
}
void Process::bring_proc_online(char* proc_id, Process* self) {
    const auto logger = spdlog::get("console");
    logger ->info("view id is: {}", self -> view_id);
    for (int i = 0; i < self -> members.size(); i++) {
        if (proc_id[i] == 't') {
            self -> members[i].alive = true;
            logger ->info("{} is alive", self -> members[i].address);
        } else self -> members[i].alive = false;
    }
}
bool Process::all_member_ack(Process* self) {
    for (const auto &n : self -> members ) {
        if (!n.alive || n.id == self->my_id) continue;
        if (!n.acknowledge) return false;
    }
    return true;
}
void *Process::start_udp_listen(void *proc) {
    const auto logger = spdlog::get("console");
    auto self = (Process *) proc;

    while (true) {
        sleep(2);

        for (auto &n: self -> members) {
            if (n.id == self -> my_id) continue;
            int sender_id = recv_msg(n.address, self);
            auto curr = std::chrono::high_resolution_clock::now();
            if (sender_id == n.id) {
                logger -> info("refreshing proc: {}'s heartbeat", sender_id);
                n.last_heartbeat_received = curr;
            }
            if (n.alive && std::chrono::duration_cast<std::chrono::seconds>(curr - n.last_heartbeat_received).count() > 10) {
                logger -> critical("Peer: {} not reachable", n.id);
            }
        }
    }
}
int Process::recv_msg(std::string addr, Process * self) {
    const auto logger = spdlog::get("console");

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[2];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, self -> udp_port.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
    }

    freeaddrinfo(servinfo);

    printf("listener: waiting to recvfrom...\n");

    addr_len = sizeof their_addr;
    if ((numbytes = recvfrom(sockfd, buf, 1024-1 , 0,
                             (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
    }

    int sender_id;
    if (numbytes > 0) sender_id = ((char *) buf)[0] - '0';
    else sender_id = -1;

//    printf("listener: got packet from %s\n",
//           inet_ntop(their_addr.ss_family,
//                     get_in_addr((struct sockaddr *)&their_addr),
//                     s, sizeof s));
//    printf("listener: packet is %d bytes long\n", numbytes);
//    printf("listener: packet id is: %d\n", sender_id);

    close(sockfd);
    return sender_id;
}
void *Process::start_udp_send(void *proc) {
    auto self = (Process *) proc;

    while (true) {
        sleep(2);

        for (const auto &n : self -> members) {
            if (n.id == self -> my_id) continue;
            send_msg(n.address, sizeof(int), self);
        }
    }
}
void Process::send_msg(std::string addr, ssize_t size, Process* self) {
    const auto logger = spdlog::get("console");

    char msg[1];
    msg[0] = '0' + self -> my_id;

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(addr.c_str(), self -> udp_port.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
    }

    if ((numbytes = sendto(sockfd, msg, sizeof(char), 0,
                           p->ai_addr, p->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }

    freeaddrinfo(servinfo);

//    printf("talker: sent %d bytes to %s\n", numbytes, addr.c_str());
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
void *Process::start_member(void * member) {
    const auto logger = spdlog::get("console");
    auto self = (Process *) member;
    int sockfd, numbytes;
    char buf[1024];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    auto leader_addr = self -> members[self -> leader_id].address;
    if ( (rv = getaddrinfo(leader_addr.c_str(), self -> port.c_str(), &hints, &servinfo)) != 0 ) {
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
    msg.proc_id = self -> my_id;
    self -> members[self -> leader_id].alive = true; // set the leader to be alive

    // ask to join the group
    join_msg* msg_to_send = hton(&msg);
    self -> curr_state = process_state::MEMBER;
    if (send(sockfd, msg_to_send, sizeof(join_msg), 0) == -1)
        logger -> error("error sending new view msg");

    freeaddrinfo(servinfo);

    while (true) {
//        logger -> info("listening, view is: {}", this -> view_id);
//        for (const auto &n : this -> members) {
//            if (n.alive) logger -> info("{} is alive", n.address);
//        }
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
                    Req_Msg* req_msg = ntoh((Req_Msg*) buf);

                    OK_Msg ok_msg;
                    ok_msg.type = 1;
                    ok_msg.peer_id = self -> my_id;

                    OK_Msg* packaged_msg = hton(&ok_msg);

                    if (send(sockfd, packaged_msg, sizeof(OK_Msg), 0) == -1)
                        logger -> error("error sending new view msg");

                    break;
                }
                case msg_type::new_view:
                {
                    new_view_msg* recved_msg = ntoh((new_view_msg *) buf);
                    self -> view_id = recved_msg -> view_id;
                    bring_proc_online(recved_msg -> member_list, self);

                    break;
                }
            }
        }

        sleep(1);
    }
}
void Process::init() {
}
