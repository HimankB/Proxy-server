#include "proxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <strings.h>  
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/tcp.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int create_listen_socket(const char *port) {
    struct addrinfo hints, *res, *p;
    int sockfd = -1;
    int optval = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;     // Use IPv6 (will also handle IPv4 with V4MAPPED)
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    // Listen on all available interfaces

    int status = getaddrinfo(NULL, port, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    // Try each address until we successfully bind
    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) {
            continue;
        }

        // Allow socket reuse
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
            perror("setsockopt SO_REUSEADDR");
            close(sockfd);
            continue;
        }

        // Allow both IPv4 and IPv6 connections on IPv6 socket
        optval = 0;  // Setting IPV6_V6ONLY to 0 allows IPv4-mapped addresses
        if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &optval, sizeof(optval)) < 0) {
            // This might fail on IPv4-only systems, which is OK
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            break;  // Success!
        }

        close(sockfd);
        sockfd = -1;
    }

    // If IPv6 bind failed, try IPv4
    if (sockfd < 0) {
        freeaddrinfo(res);
        
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;      // IPv4 only
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        status = getaddrinfo(NULL, port, &hints, &res);
        if (status != 0) {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
            return -1;
        }

        for (p = res; p != NULL; p = p->ai_next) {
            sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (sockfd < 0) continue;

            optval = 1;
            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

            if (bind(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
                break;
            }

            close(sockfd);
            sockfd = -1;
        }
    }

    freeaddrinfo(res);

    if (sockfd < 0) {
        fprintf(stderr, "Failed to bind to port %s\n", port);
        return -1;
    }

    if (listen(sockfd, 10) < 0) {
        perror("listen failed");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// Case-insensitive string search
char* strcasestr_custom(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    
    size_t needle_len = strlen(needle);
    if (needle_len == 0) return (char*)haystack;
    
    for (const char *p = haystack; *p; p++) {
        if (strncasecmp(p, needle, needle_len) == 0) {
            return (char*)p;
        }
    }
    return NULL;
}

// Helper function to find a substring in binary data
void* memmem_custom(const void *haystack, size_t haystack_len, 
                    const void *needle, size_t needle_len) {
    if (needle_len == 0) return (void*)haystack;
    if (haystack_len < needle_len) return NULL;
    
    const char *h = haystack;
    const char *n = needle;
    
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(h + i, n, needle_len) == 0) {
            return (void*)(h + i);
        }
    }
    return NULL;
}

ssize_t read_full_request(int client_fd, char *request, size_t max_len);
int parse_request(const char *request, char *uri, char *host);
void print_request_tail(const char *request);
void adjust_actual_host_uri(const char *host, const char *uri, char *actual_host, char *actual_uri, char *print_uri);
int connect_to_target(const char *actual_host, int *server_fd);
int forward_request_to_server(int server_fd, const char *request, size_t request_len);
void relay_response(int client_fd, int server_fd);

void handle_client(int client_fd) {
    char request[BUF_SIZE * 4];  // Large buffer for request
    size_t request_len = read_full_request(client_fd, request, sizeof(request));
    if (request_len <= 0) return;  // closed or error already handled

    request[request_len] = '\0';

    char host[256] = {0}, uri[1024] = {0};
    if (!parse_request(request, uri, host)) {
        close(client_fd);
        return;
    }

    print_request_tail(request);

    char actual_host[256], actual_uri[1024], print_uri[1024];
    adjust_actual_host_uri(host, uri, actual_host, actual_uri, print_uri);

    printf("GETting %s %s\n", host, print_uri);
    fflush(stdout);

    int server_fd;
    if (connect_to_target(actual_host, &server_fd) != 0) {
        close(client_fd);
        return;
    }

    if (forward_request_to_server(server_fd, request, request_len) != 0) {
        close(server_fd);
        close(client_fd);
        return;
    }

    relay_response(client_fd, server_fd);

    close(server_fd);
    close(client_fd);
}

// Reads until "\r\n\r\n" or error/close; returns total bytes read (excluding terminator byte)
ssize_t read_full_request(int client_fd, char *request, size_t max_len) {
    size_t len = 0;
    while (len < max_len - 1) {
        ssize_t bytes = recv(client_fd, request + len,
                             max_len - len - 1, 0);
        if (bytes <= 0) {
            close(client_fd);
            return -1;
        }
        len += bytes;
        if (memmem_custom(request, len, "\r\n\r\n", 4)) {
            break;
        }
    }
    return len;
}

// Parses the request line and Host header; fills uri and host buffers; returns 1 on success
int parse_request(const char *request, char *uri, char *host) {
    const char *request_line_end = strstr(request, "\r\n");
    if (!request_line_end) {
        fprintf(stderr, "Invalid HTTP request - no line ending\n");
        return 0;
    }

    if (sscanf(request, "GET %1023s HTTP/1.1", uri) != 1) {
        fprintf(stderr, "Invalid HTTP request\n");
        return 0;
    }

    const char *headers_start = request_line_end + 2;
    char *host_line = strcasestr_custom(headers_start, "Host:");
    if (!host_line) {
        fprintf(stderr, "No Host header found\n");
        return 0;
    }

    // Extract host value
    char *colon = strchr(host_line, ':');
    if (!colon) return 0;
    char *hs = colon + 1;
    while (*hs == ' ' || *hs == '\t') hs++;
    size_t hlen = 0;
    while (hs[hlen] && hs[hlen] != '\r' && hs[hlen] != '\n') hlen++;
    if (hlen >= 256) hlen = 255;
    strncpy(host, hs, hlen);
    host[hlen] = '\0';
    while (hlen > 0 && (host[hlen-1] == ' ' || host[hlen-1] == '\t')) {
        host[--hlen] = '\0';
    }
    return 1;
}

// Finds and prints the very last header line before the blank line
void print_request_tail(const char *request) {
    const char *headers_end = strstr(request, "\r\n\r\n");
    if (!headers_end) return;

    // find start of last header line
    const char *line_start = headers_end - 2;
    while (line_start > request && *(line_start - 1) != '\n') {
        line_start--;
    }

    // find end of that line
    const char *line_end = line_start;
    while (*line_end && *line_end != '\r' && *line_end != '\n') {
        line_end++;
    }

    size_t raw_len = line_end - line_start;
    if (raw_len == 0) return;

    // --- DEBUG: dump raw bytes ---
    fprintf(stderr, "DEBUG raw tail (%zu bytes): ", raw_len);
    for (size_t i = 0; i < raw_len; i++) {
        fprintf(stderr, "%02x ", (unsigned char)line_start[i]);
    }
    fprintf(stderr, "\n");

    // trim trailing CR/spaces/tabs
    size_t trim_len = raw_len;
    while (trim_len > 0 &&
           (line_start[trim_len - 1] == '\r' ||
            line_start[trim_len - 1] == ' '  ||
            line_start[trim_len - 1] == '\t')) {
        trim_len--;
    }

    // --- DEBUG: dump trimmed bytes ---
    fprintf(stderr, "DEBUG trimmed tail (%zu bytes): ", trim_len);
    for (size_t i = 0; i < trim_len; i++) {
        fprintf(stderr, "%02x ", (unsigned char)line_start[i]);
    }
    fprintf(stderr, "\n");

    // copy into a buffer and print normally
    char buf[1024];
    if (trim_len >= sizeof(buf)) trim_len = sizeof(buf) - 1;
    memcpy(buf, line_start, trim_len);
    buf[trim_len] = '\0';

    printf("Request tail %s\n", buf);
    fflush(stdout);
}

// Adjusts actual_host, actual_uri and print_uri when URI contains full URL
void adjust_actual_host_uri(const char *host, const char *uri, char *actual_host, char *actual_uri, char *print_uri)
{
    strcpy(actual_host, host);
    strcpy(actual_uri, uri);
    strcpy(print_uri, uri);

    if (strncmp(uri, "http://", 7) == 0) {
        strcpy(print_uri, uri);
        const char *uhs = uri + 7;
        const char *uhe = strchr(uhs, '/');
        if (uhe) {
            size_t hlen = uhe - uhs;
            if (hlen < 256) {
                strncpy(actual_host, uhs, hlen);
                actual_host[hlen] = '\0';
                strcpy(actual_uri, uhe);
            }
        } else {
            strcpy(actual_uri, "/");
        }
    }
}


// Resolves and connects to actual_host, sets server_fd; returns 0 on success
int connect_to_target(const char *actual_host, int *server_fd) {
    struct addrinfo hints = {0}, *res, *p;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char hostname[256];
    char port_str[6] = "80";
    strncpy(hostname, actual_host, sizeof(hostname)-1);
    hostname[sizeof(hostname)-1] = '\0';
    char *colon = strchr(hostname, ':');
    if (colon) {
        *colon = '\0';
        strncpy(port_str, colon+1, 5);
        port_str[5] = '\0';
    }

    int status = getaddrinfo(hostname, port_str, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo failed for %s: %s\n", hostname, gai_strerror(status));
        return -1;
    }

    int fd = -1;
    for (p = res; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) {
        fprintf(stderr, "Failed to connect to %s:%s\n", hostname, port_str);
        return -1;
    }
    *server_fd = fd;
    return 0;
}

// Sends the entire request buffer to the server; returns 0 on success
int forward_request_to_server(int server_fd, const char *request, size_t request_len) {
    size_t sent_total = 0;
    while (sent_total < request_len) {
        ssize_t sent = send(server_fd, request + sent_total,
                            request_len - sent_total, MSG_NOSIGNAL);
        if (sent < 0) {
            perror("send to server failed");
            return -1;
        }
        sent_total += sent;
    }
    return 0;
}

// Relays the server response back to client, printing Content-Length once
void relay_response(int client_fd, int server_fd) {
    char response_buffer[BUF_SIZE];
    char header_buffer[BUF_SIZE * 2] = {0};
    size_t header_len = 0;
    int headers_complete = 0;
    int content_length_printed = 0;

    while (1) {
        ssize_t bytes = recv(server_fd, response_buffer, BUF_SIZE, 0);
        if (bytes <= 0) {
            if (bytes < 0) perror("recv from server failed");
            break;
        }

        if (!headers_complete && header_len + bytes < sizeof(header_buffer)) {
            memcpy(header_buffer + header_len, response_buffer, bytes);
            header_len += bytes;
            char *he = strstr(header_buffer, "\r\n\r\n");
            if (he) {
                headers_complete = 1;
                if (!content_length_printed) {
                    char *cl = strcasestr_custom(header_buffer, "Content-Length:");
                    if (cl && cl < he) {
                        int content_length;
                        if (sscanf(cl, "Content-Length: %d", &content_length) == 1) {
                            printf("Response body length %d\n", content_length);
                            fflush(stdout);
                            content_length_printed = 1;
                        }
                    }
                }
            }
        }

        size_t sent_total = 0;
        while (sent_total < bytes) {
            ssize_t sent = send(client_fd, response_buffer + sent_total,
                                bytes - sent_total, MSG_NOSIGNAL);
            if (sent < 0) {
                if (errno == EPIPE || errno == ECONNRESET)
                    return;
                perror("send to client failed");
                return;
            }
            sent_total += sent;
        }
    }
}
