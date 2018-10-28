#include <iostream>
#include "include/spdlog/spdlog.h"
#include "include/spdlog/sinks/stdout_color_sinks.h"
#include "parse.h"
#include "process.h"

int main(int argc, char **argv) {
    auto console = spdlog::stdout_color_mt("console");
    console -> info("welcome!");

    std::vector<std::string> addr_book;
    std::string port;

    std::tie(addr_book, port) = handle_input(argc, argv);
    Process *process = new Process(addr_book, port);
    process -> init();

    return 0;
}