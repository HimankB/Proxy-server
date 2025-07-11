/* proxy.h */
#ifndef PROXY_H
#define PROXY_H

#include <sys/types.h>
#define BUF_SIZE 8192

int create_listen_socket(const char *port);
void handle_client(int client_fd);
void error_exit(const char *msg);
char* strcasestr_custom(const char *haystack, const char *needle);
void* memmem_custom(const void *haystack, size_t haystack_len,
                    const void *needle, size_t needle_len);

#endif
