//
// Created by Linghan Xing on 10/27/18.
//

#ifndef PRJ2_PARSE_H
#define PRJ2_PARSE_H

#include <tuple>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <string>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fstream>
#include <iostream>
#include <unordered_map>

std::tuple<std::vector<std::string>, std::string> handle_input(int argc, char **argv);

#endif //PRJ2_PARSE_H
