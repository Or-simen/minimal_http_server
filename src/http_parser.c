#include "server.h"
#include <stdio.h>

int parse_request(const char *raw_request, HttpRequest *req) {
  // Bounded scanning of the top HTTP Request-Line
  int scanned = sscanf(raw_request, "%15s %511s %15s", req->method, req->path,
                       req->version);
  if (scanned < 2) {
    return -1;
  }
  return 0;
}
