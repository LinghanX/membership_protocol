#include <iostream>
#include "include/spdlog/spdlog.h"
#include "include/spdlog/sinks/stdout_color_sinks.h"
#include "parse.h"
#include "process.h"
#include <pthread.h>

int main(int argc, char **argv) {
    auto console = spdlog::stdout_color_mt("console");
    console -> info("welcome!");

    std::vector<std::string> addr_book;
    std::string port;

    std::tie(addr_book, port) = handle_input(argc, argv);
    Process *process = new Process(addr_book, port);

    auto logger = spdlog::get("console");

    int rc1, rc2, rc3;
    pthread_t tcp_thread, udp_listen_thread, udp_send_thread;
    if (process -> my_id == process -> leader_id) {
        if ( rc1 = pthread_create(&tcp_thread, NULL, &Process::start_leader, process) ) {
        }
    } else {
        if ( rc1 = pthread_create(&tcp_thread, NULL, &Process::start_member, process)) {}
    }
    rc2 = pthread_create(&udp_send_thread, NULL, &Process::start_udp_send, process);

    sleep(1);
    rc3 = pthread_create(&udp_listen_thread, NULL, &Process::start_udp_listen, process);

    pthread_join(tcp_thread, NULL);
    pthread_join(udp_listen_thread, NULL);
    pthread_join(udp_send_thread, NULL);

    return 0;
}