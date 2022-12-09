/**
 * @file CSftp.c
 * Entry point for FTP server program: creates server socket listening to a
 * given port and accepts connections
 * 
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ftpservice.h"
#include "usage.h"

char root_directory[PATH_LEN];
uint8_t hostip_octets[4];
cmd_map_t cmd_map[10] = {
    {"USER", CMD_USER},
    {"QUIT", CMD_QUIT},
    {"CWD", CMD_CWD},
    {"CDUP", CMD_CDUP},
    {"TYPE", CMD_TYPE},
    {"MODE", CMD_MODE},
    {"STRU", CMD_STRU},
    {"RETR", CMD_RETR},
    {"PASV", CMD_PASV},
    {"NLST", CMD_NLST}
};

/**
 * Get port number from str
 *
 * @param str
 * @return port number if given port str is valid; else -1
 */
int get_port(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit(str[i])) {
            return -1;
        }
    }
    int port = atoi(str);
    if (!is_valid_port(port)) {
        return -1;
    }
    return port;
}

/**
 * Finds IP address of host machine and sets host IP octets
 * https://stackoverflow.com/questions/212528/how-can-i-get-the-ip-address-of-a-linux-machine
 *
 */
void set_hostip() {
    struct ifaddrs *if_addrs = NULL;
    void *sin_addr = NULL;
    char address[INET_ADDRSTRLEN];

    getifaddrs(&if_addrs);
    for (struct ifaddrs *ifa = if_addrs; ifa != NULL; ifa = ifa->ifa_next) {
        // Check it is an IPv4 address
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        sin_addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
        inet_ntop(AF_INET, sin_addr, address, INET_ADDRSTRLEN);
        // Skip localhost
        if (strncmp("127", address, 3) == 0) {
            continue;
        }
        char *token = strtok(address, ".");
        for (int i = 0; i < 4; i++) {
            if (token == NULL) {
                break;
            }
            hostip_octets[i] = atoi(token);
            token = strtok(NULL, ".");
        }
        break;
    }
    
    if (if_addrs != NULL) {
        freeifaddrs(if_addrs);
    }
}

int main(int argc, char **argv) {
    int port;
    if (argc != 2 || (port = get_port(argv[1])) == -1) {
        usage(argv[0]);
        return 0;
    }

    // Set current working directory as program root directory
    getcwd(root_directory, sizeof(root_directory));

    // Set IP of host machine
    set_hostip();

    // Start server on port
    socket_t server_socket;
    if (!open_port(port, &server_socket)) {
        return 0;
    }
    printf("Listening on %d.%d.%d.%d:%d\n",
           hostip_octets[0],
           hostip_octets[1],
           hostip_octets[2],
           hostip_octets[3],
           port);

    // Accept clients
    pthread_t child;
    int clientfd;
    while (1) {
        clientfd = accept(
            server_socket.fd,
            (struct sockaddr *) &server_socket.addr,
            &server_socket.addrlen
        );
        pthread_create(&child, NULL, handle_session, (void*) &clientfd);
        printf("FTP session opened (connect).\n");
        pthread_join(child, NULL);
    }
    return 0;
}