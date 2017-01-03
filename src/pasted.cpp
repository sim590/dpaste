/*
 * Copyright © 2016 Simon Désaulniers
 * Author: Simon Désaulniers <sim.desaulniers@gmail.com>
 *
 * This file is part of dpaste.
 *
 * dpaste is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dpaste is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dpaste.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "server.h"

#include <iostream>
#include <string>
#include <getopt.h>
#include <signal.h>
#include <condition_variable>
#include <mutex>

std::mutex signal_mtx;
std::condition_variable signal_cv;

void signal_handler(int sig)
{
    switch(sig) {
    case SIGTERM:
    case SIGINT:
        signal_cv.notify_all();
        break;
    }
}


/* Command line parsing */
struct ParsedArgs {
    bool fail {false};
    bool help {false};
    bool version {false};
	uint16_t port {dpaste::ipc::Server::DEFAULT_SERVER_PORT};
};

static const constexpr struct option long_options[] = {
   {"help",       no_argument      , nullptr, 'h'},
   {"version",    no_argument      , nullptr, 'v'},
   {"port",		  required_argument, nullptr, 'p'},
   {nullptr,      0                , nullptr,  0}
};

ParsedArgs parseArgs(int argc, char *argv[]) {
    ParsedArgs pa;
    int opt;
    while ((opt = getopt_long(argc, argv, "hvp:", long_options, nullptr)) != -1) {
        switch (opt) {
        case 'h':
            pa.help = true;
            break;
        case 'p': {
            uint16_t port;
            std::istringstream iss {optarg};
            iss >> port;
            if (iss.fail()) {
                std::cerr << "'-p' flag has invalid arg..." << std::endl;
                pa.fail = true;
                return pa;
            } else
                pa.port = port;
            break;
        }
        case 'v':
            pa.version = true;
            break;
        default:
            pa.fail = true;
            return pa;
        }
    }
    return pa;
}

void print_help() {
    std::cout << "pasted -- Daemon for dpaste." << std::endl << std::endl;

    std::cout << "SYNOPSIS" << std::endl
              << "    dpaste [-h]" << std::endl
              << "    dpaste [-p port]" << std::endl;

    std::cout << "OPTIONS" << std::endl;
    std::cout << "    -h|--help" << std::endl
              << "        Prints this help text." << std::endl;

    std::cout << "    -p|--port {port}" << std::endl
              << "        Run the daemon on port {port}." << std::endl;
}

int main(int argc, char *argv[]) {
    auto parsed_args = parseArgs(argc, argv);
    if (parsed_args.fail) {
        return -1;
    } else if (parsed_args.help) {
        print_help();
        return 0;
    } else if (parsed_args.version) {
        std::cout << VERSION << std::endl;
        return 0;
    }

    dpaste::ipc::Server server;
    try {
        server.run(parsed_args.port);
    } catch (dpaste::ipc::ServerInitException& e) {
        return -1;
    }

    {
        std::unique_lock<std::mutex> lk(signal_mtx);
        signal(SIGINT, signal_handler);
        signal_cv.wait(lk);
    }
    server.stop();
    return 0;
}

