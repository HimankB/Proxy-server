# HTTP Proxy Server

A simple HTTP/1.1 proxy server implementation in C that forwards client requests to target web servers and relays responses back to clients. This proxy supports both IPv4 and IPv6 connections.

## Overview

This proxy server acts as an intermediary between HTTP clients (like web browsers) and web servers. It receives HTTP GET requests from clients, forwards them to the appropriate destination servers, and relays the responses back to the clients.

## Features

- **Dual-stack networking**: Supports both IPv4 and IPv6 connections
- **HTTP/1.1 protocol**: Handles standard HTTP GET requests
- **Request forwarding**: Parses and forwards complete HTTP requests
- **Response relaying**: Streams server responses back to clients
- **Diagnostic output**: Prints request details and response metadata
- **Error handling**: Gracefully handles connection errors and malformed requests
- **Signal handling**: Ignores SIGPIPE to prevent crashes from broken connections

## Architecture

### Main Components

1. **main.c**: Entry point and main server loop
   - Initializes listening socket
   - Accepts incoming client connections
   - Dispatches each connection to the handler

2. **proxy.c**: Core proxy functionality
   - Request parsing and forwarding
   - Response relaying
   - Connection management
   - Diagnostic output

3. **proxy.h**: Header file with function declarations and constants

### Request Flow

```
Client → Proxy → Target Server
         ↓           ↓
      Parse      Forward
      Request    Request
         ↓           ↓
      Relay ← Receive
      Response  Response
         ↓
      Client
```

## Usage

### Compilation

```bash
gcc -o proxy main.c proxy.c -Wall -Wextra
```

### Running the Proxy

```bash
./proxy -p <port>
```

**Example**:
```bash
./proxy -p 8080
```

This starts the proxy server listening on port 8080.

### Testing the Proxy

Configure your web browser or HTTP client to use the proxy:
- Proxy host: `localhost` (or your machine's IP)
- Proxy port: `8080` (or your chosen port)

Or test with curl:
```bash
curl -x http://localhost:8080 http://example.com
```

## Implementation Details

### Socket Creation (`create_listen_socket`)

Creates a listening socket with the following features:
- Attempts IPv6 binding first (with IPv4-mapped address support)
- Falls back to IPv4 if IPv6 is unavailable
- Enables `SO_REUSEADDR` for quick restarts
- Configures `IPV6_V6ONLY=0` to accept both IPv4 and IPv6 connections

### Request Handling (`handle_client`)

For each client connection:
1. **Read**: Receives complete HTTP request (until `\r\n\r\n`)
2. **Parse**: Extracts URI and Host header
3. **Connect**: Establishes connection to target server
4. **Forward**: Sends complete request to target server
5. **Relay**: Streams response back to client
6. **Cleanup**: Closes both connections

### Request Parsing (`parse_request`)

Parses HTTP requests to extract:
- **Request URI**: The path being requested
- **Host header**: The target server hostname

Supports both:
- Relative URIs: `GET /path HTTP/1.1` with `Host: example.com`
- Absolute URIs: `GET http://example.com/path HTTP/1.1`

### URL Handling (`adjust_actual_host_uri`)

Handles different URI formats:
- **Relative URI**: `/path/to/resource`
- **Absolute URI**: `http://example.com/path/to/resource`

Extracts the actual hostname and path for connection and forwarding.

### Connection Management

**Client Connections**:
- Accepts connections on IPv4/IPv6 listening socket
- Reads entire request before processing
- Sends response data as it arrives from server

**Server Connections**:
- Resolves hostname using `getaddrinfo()`
- Supports custom ports (default 80)
- Attempts all resolved addresses until successful
- Uses `MSG_NOSIGNAL` to handle broken pipes gracefully

### Response Handling (`relay_response`)

Relays server response with additional processing:
- **Header parsing**: Extracts `Content-Length` header
- **Diagnostic output**: Prints response body length
- **Streaming**: Forwards data in chunks without buffering entire response
- **Error handling**: Detects broken pipes and connection resets

## Diagnostic Output

The proxy prints the following information for each request:

```
Accepted
Request tail <last-header-line>
GETting <host> <uri>
Response body length <size>
```

**Example**:
```
Accepted
Request tail User-Agent: curl/7.68.0
GETting example.com /index.html
Response body length 1256
```

### Debug Output

When debugging is enabled, the proxy prints:
- Raw bytes of last header line (hexadecimal)
- Trimmed bytes after removing whitespace

## Configuration

### Buffer Sizes

```c
#define BUF_SIZE 4096  // Standard I/O buffer
```

Request buffer: `BUF_SIZE * 4` (16KB)  
Response buffer: `BUF_SIZE` (4KB)

### Limits

- Maximum URI length: 1023 characters
- Maximum hostname length: 255 characters
- Listen backlog: 10 connections
- No connection timeout (blocks indefinitely)

## Error Handling

The proxy handles various error conditions:

| Error | Behavior |
|-------|----------|
| Invalid HTTP request | Close connection, log error |
| Missing Host header | Close connection, log error |
| DNS resolution failure | Close connection, log error |
| Connection to server fails | Close connection, log error |
| Send/receive errors | Close connections, continue serving |
| Client disconnects | Close both connections gracefully |

## Limitations

### Current Limitations

1. **HTTP methods**: Only GET requests are supported
2. **Protocol**: HTTP/1.1 only (no HTTPS/TLS support)
3. **Concurrency**: Sequential request handling (one at a time)
4. **Persistence**: No keep-alive support
5. **Caching**: No response caching
6. **Authentication**: No proxy authentication
7. **Content**: No content filtering or modification

### Security Considerations

⚠️ **This proxy is for educational purposes only**

- No authentication or access control
- No HTTPS/SSL support (plain text only)
- No protection against malicious requests
- No rate limiting or resource controls
- Should not be exposed to public internet

## Potential Enhancements

**Concurrency**:
- Multi-threading with `pthread`
- Process forking for parallel requests
- `select()`/`poll()`/`epoll()` for async I/O

**Features**:
- HTTPS support with OpenSSL/TLS
- HTTP methods: POST, PUT, DELETE, etc.
- Connection keep-alive
- Response caching with cache headers
- Request/response logging to file

**Robustness**:
- Timeout handling for slow servers
- Maximum request/response size limits
- Better error recovery and reporting
- Configuration file support

**Security**:
- Basic/digest authentication
- Access control lists (ACLs)
- URL filtering and blacklisting
- Request sanitization

## Platform Compatibility

- **Linux**: Full support
- **macOS**: Full support
- **BSD**: Should work with minor modifications
- **Windows**: Requires Winsock adaptation

## Dependencies

Standard C libraries:
- `stdio.h`, `stdlib.h`, `string.h`
- `unistd.h` (POSIX)
- `sys/socket.h`, `netinet/in.h`, `arpa/inet.h`
- `netdb.h` (DNS resolution)
- `signal.h` (signal handling)

## Building and Testing

### Build
```bash
gcc -o proxy main.c proxy.c -Wall -Wextra -std=c99
```

### Run
```bash
./proxy -p 8888
```

### Test
```bash
# In another terminal
curl -x http://localhost:8888 http://example.com
curl -x http://localhost:8888 http://info.cern.ch

# With verbose output
curl -v -x http://localhost:8888 http://httpbin.org/get
```

## Troubleshooting

**Port already in use**:
```
bind: Address already in use
```
→ Choose a different port or wait for OS to release the port

**Connection refused**:
```
Failed to connect to <host>:<port>
```
→ Target server may be down or blocking proxy connections

**DNS resolution failure**:
```
getaddrinfo failed for <host>
```
→ Check hostname spelling and DNS connectivity

## License

Educational/Academic use

## Author

Network programming exercise demonstrating HTTP proxy implementation
