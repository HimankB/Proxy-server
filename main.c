#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <signal.h>
#include "proxy.h"

int main(int argc, char *argv[]) {
    // Ignore SIGPIPE to prevent crashes when remote closes connection
    signal(SIGPIPE, SIG_IGN);
    
    if (argc != 3 || strcmp(argv[1], "-p") != 0) {
        fprintf(stderr, "Usage: %s -p <port>\n", argv[0]);
        exit(1);
    }

    int listen_fd = create_listen_socket(argv[2]);
    if (listen_fd < 0) {
        fprintf(stderr, "Failed to create listening socket\n");
        exit(1);
    }

    while (1) {
        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_size);
        
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }
        
        printf("Accepted\n");
        fflush(stdout);
        
        handle_client(client_fd);
    }

    close(listen_fd);
    return 0;
}
