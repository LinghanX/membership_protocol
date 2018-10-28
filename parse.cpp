//
// Created by Linghan Xing on 10/27/18.
//

#include "parse.h"

std::vector<std::string> parsePath(std::string paths) {
    std::vector<std::string> addr_book;
    std::ifstream file(paths);
    std::string str;
    int counter = 1;
    while (std::getline(file, str))
    { addr_book.push_back(str); }

    file.close();

    return addr_book;
}

std::tuple<std::vector<std::string>, std::string> handle_input(int argc, char **argv)
{
    auto logger = spdlog::get("console");

    int option;
    int pflag = 0;
    int hflag = 0;
    std::string portNum;
    char *path;
    std::vector<std::string> neighbors;

    while ((option = getopt(argc, argv, "p:h:")) != -1) {
        switch (option) {
            case 'p':
                portNum = optarg;
                pflag = 1;
                break;
            case 'h':
                path = optarg;
                neighbors = parsePath(path);
                hflag = 1;
                break;
            case '?':
                if (optopt == 'p')
                    fprintf(stderr, "Option -%c needs argument\n", optopt);
                else fprintf(stderr, "Unknown option -%c. \n", optopt);
                break;
            default:
                fprintf(stderr, "error");
        }
    }


    if (hflag == 0 || pflag == 0) {
        logger -> error("please input all required options");
        exit(1);
    }

    if (stoi(portNum) < 1024 ||  stoi(portNum) > 65535) {
        logger -> error("PortNumber out of range", portNum);
        exit(1);
    }

    logger -> info("finished processing input");
    logger -> info("the selected portal is: {}", portNum);
    logger -> info("the selected file path is: {}", path);

    return std::make_tuple(neighbors, portNum);
}



