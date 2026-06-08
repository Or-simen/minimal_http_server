#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>

#define PORT 8080
#define BACKLOG 10
#define BUFFER_SIZE 4096

typedef struct {
  char method[16];
  char path[512];
  char version[16];
} HttpRequest;

void handle_client(int client_fd, const char *dir_path);
int parse_request(const char *raw_request, HttpRequest *req);

#endif
