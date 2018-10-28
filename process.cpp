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
    this -> curr_state = this -> my_id == 0
            ? process_state::LEADER : process_state::NON_MEMBER;

//    logger -> info("finished processing args");
//    logger -> info("I am a {}", this -> curr_state);
//
//    for (const auto& n : this -> members) {
//        logger -> info("member {} is: {}, addr is: {}", n.id, n.alive, n.address);
//    }
    init();
}

void *get_in_addr(struct sockaddr *sa)
{
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
    char buffer[1024];

    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, this -> port.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, 10) == -1) {
        perror("listen");
        exit(1);
    }

    printf("server: waiting for connections...\n");
    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got connection from %s\n", s);

        ssize_t recv_bytes = recv(new_fd, buffer, 1024, 0);
        if (recv_bytes > 0) {
            msg_type type = check_msg_type(buffer, recv_bytes);

            close(new_fd); // dont need the connection anymore
            switch (type) {
                case msg_type::join:
                {
                    join_msg* recved_msg = ntoh((join_msg *) buffer);
                    Req_Msg msg;
                    msg.view_id = this ->view_id;
                    msg.type = OPERATION::ADD;
                    msg.peer_id = recved_msg -> proc_id;
                    this -> members[this -> my_id].acknowledge = true;
                    broadcast_req_msg(&msg);

                    break;
                }
                case msg_type::ok:
                {
                    OK_Msg* recved_msg = ntoh((OK_Msg *) buffer);
                    int id = recved_msg -> peer_id;
                    this -> members[id].acknowledge = true;
                    if (all_member_ack()) {
                        init_new_view();
                    }
                    break;
                }
            }
        }
    }
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
    printf("client: connecting to %s\n", s);
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

    if (size == sizeof(join_msg) && *first_int == 4) {
        return msg_type::join;
    } else if (size == sizeof(OK_Msg) && *first_int == 1){
        return msg_type::ok;
    } else {
        logger -> error("unable to identify incoming message");
        return msg_type::unknown;
    }
}
void Process::handle_message(int sockfd) {
}
void Process::idle() {}

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
                idle();
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
