#include "server.h"
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

// Decodes percent-encoded URLs (e.g., "hello%20world.txt" -> "hello world.txt")
void url_decode(char *dst, const char *src) {
  char a, b;
  while (*src) {
    if ((*src == '%') && ((a = src[1]) && (b = src[2])) &&
        (isxdigit((unsigned char)a) && isxdigit((unsigned char)b))) {
      if (a >= 'a')
        a -= 'a' - 'A';
      if (a >= 'A')
        a -= 'A' - 10;
      else
        a -= '0';
      if (b >= 'a')
        b -= 'a' - 'A';
      if (b >= 'A')
        b -= 'A' - 10;
      else
        b -= '0';
      *dst++ = 16 * a + b;
      src += 3;
    } else if (*src == '+') {
      *dst++ = ' ';
      src++;
    } else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';
}

void send_html_response(int client_fd, const char *status, const char *body) {
  char header[BUFFER_SIZE];
  snprintf(header, sizeof(header),
           "HTTP/1.1 %s\r\n"
           "Content-Type: text/html; charset=utf-8\r\n"
           "Content-Length: %zu\r\n"
           "Connection: close\r\n\r\n",
           status, strlen(body));
  send(client_fd, header, strlen(header), 0);
  send(client_fd, body, strlen(body), 0);
}

void serve_directory(int client_fd, const char *full_path,
                     const char *req_path) {
  DIR *dir = opendir(full_path);
  if (!dir) {
    send_html_response(client_fd, "500 Internal Server Error",
                       "<h1>500 Internal Server Error</h1>");
    return;
  }

  char body[16384] = {0};
  strcat(body, "<html><head><style>body{font-family:sans-serif;padding:20px;}</"
               "style><title>Index</title></head><body>");
  snprintf(body + strlen(body), sizeof(body) - strlen(body),
           "<h1>Index of %s</h1><hr><ul>", req_path);

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0)
      continue;

    char link[1024];
    if (strcmp(req_path, "/") == 0) {
      snprintf(link, sizeof(link), "/%s", entry->d_name);
    } else {
      snprintf(link, sizeof(link), "%s/%s", req_path, entry->d_name);
    }

    snprintf(body + strlen(body), sizeof(body) - strlen(body),
             "<li><a href=\"%s\">%s%s</a></li>", link, entry->d_name,
             (entry->d_type == DT_DIR) ? "/" : "");
  }
  closedir(dir);
  strcat(body, "</ul></body></html>");

  send_html_response(client_fd, "200 OK", body);
}

// Helper to determine content type based on file extension
const char *get_content_type(const char *path) {
  const char *ext = strrchr(path, '.');
  if (!ext)
    return "text/plain"; // default fallback for files with no extension

  if (strcmp(ext, ".css") == 0)
    return "text/css; charset=utf-8";

  if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
    return "text/html; charset=utf-8";
  if (strcmp(ext, ".txt") == 0 || strcmp(ext, ".c") == 0 ||
      strcmp(ext, ".h") == 0)
    return "text/plain; charset=utf-8";
  if (strcmp(ext, ".png") == 0)
    return "image/png";
  if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
    return "image/jpeg";
  if (strcmp(ext, ".pdf") == 0)
    return "application/pdf";

  return "application/octet-stream"; // fallback for binary files
}

void serve_file(int client_fd, const char *full_path) {
  int file_fd = open(full_path, O_RDONLY);
  if (file_fd < 0) {
    send_html_response(client_fd, "404 Not Found", "<h1>404 Not Found</h1>");
    return;
  }

  const char *content_type = get_content_type(full_path);

  char header[512];
  snprintf(header, sizeof(header),
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: %s\r\n"
           "Connection: close\r\n\r\n",
           content_type);
  send(client_fd, header, strlen(header), 0);

  char file_buf[BUFFER_SIZE];
  ssize_t bytes_read;
  while ((bytes_read = read(file_fd, file_buf, sizeof(file_buf))) > 0) {
    send(client_fd, file_buf, bytes_read, 0);
  }
  close(file_fd);
}

void handle_client(int client_fd, const char *dir_path) {
  char buffer[BUFFER_SIZE] = {0};
  ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
  if (bytes_received <= 0) {
    close(client_fd);
    return;
  }

  HttpRequest req;
  if (parse_request(buffer, &req) < 0) {
    send_html_response(client_fd, "400 Bad Request",
                       "<h1>400 Bad Request</h1>");
    close(client_fd);
    return;
  }

  // Direct mitigation against directory traversal vulnerabilities (e.g.,
  // ../../../etc/passwd)
  if (strstr(req.path, "..")) {
    send_html_response(client_fd, "403 Forbidden", "<h1>403 Forbidden</h1>");
    close(client_fd);
    return;
  }

  char decoded_path[512];
  url_decode(decoded_path, req.path);

  char full_path[1024];
  snprintf(full_path, sizeof(full_path), "%s%s", dir_path, decoded_path);

  struct stat path_stat;
  if (stat(full_path, &path_stat) < 0) {
    send_html_response(client_fd, "404 Not Found", "<h1>404 Not Found</h1>");
  } else if (S_ISDIR(path_stat.st_mode)) {
    serve_directory(client_fd, full_path, decoded_path);
  } else if (S_ISREG(path_stat.st_mode)) {
    serve_file(client_fd, full_path);
  } else {
    send_html_response(client_fd, "400 Bad Request",
                       "<h1>Unsupported Asset Type</h1>");
  }

  close(client_fd);
}
