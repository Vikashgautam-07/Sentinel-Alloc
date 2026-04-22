/**
 * @file main.c
 * @brief REST API server for memory allocator
 * @details HTTP server providing endpoints to interact with allocator via GUI
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "allocator.h"

#define SERVER_PORT 8081
#define LISTEN_BACKLOG 10
#define BUFFER_SIZE 4096
#define WEB_ROOT "web"

/* Global allocator instance */
static Allocator *g_allocator = NULL;
static int g_server_socket = -1;
static bool g_running = true;

/* Request handler thread info */
typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} ClientInfo;

static const char *get_reason_phrase(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

static bool send_all(int socket, const char *data, size_t length) {
    size_t sent_total = 0;

    while (sent_total < length) {
        ssize_t sent = send(socket, data + sent_total, length - sent_total, 0);
        if (sent <= 0) {
            return false;
        }
        sent_total += (size_t)sent;
    }

    return true;
}

/* HTTP Response helpers */
static void send_http_response(int socket, int status_code, const char *content_type, const char *body) {
    char header[1024];
    const char *reason = get_reason_phrase(status_code);

    int header_length = snprintf(header, sizeof(header),
                                 "HTTP/1.1 %d %s\r\n"
                                 "Content-Type: %s\r\n"
                                 "Content-Length: %zu\r\n"
                                 "Access-Control-Allow-Origin: *\r\n"
                                 "Access-Control-Allow-Headers: Content-Type\r\n"
                                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                                 "Connection: close\r\n"
                                 "\r\n",
                                 status_code, reason, content_type, strlen(body));

    if (header_length > 0 && (size_t)header_length < sizeof(header)) {
        send_all(socket, header, (size_t)header_length);
        send_all(socket, body, strlen(body));
    }
}

static void send_file_response(int client_socket, const char *file_path, const char *content_type) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        send_http_response(client_socket, 404, "text/plain", "Not Found");
        return;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        send_http_response(client_socket, 500, "text/plain", "Failed to read file");
        return;
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        send_http_response(client_socket, 500, "text/plain", "Failed to read file");
        return;
    }

    rewind(file);

    char *buffer = malloc((size_t)file_size + 1);
    if (!buffer) {
        fclose(file);
        send_http_response(client_socket, 500, "text/plain", "Failed to allocate buffer");
        return;
    }

    size_t bytes_read = fread(buffer, 1, (size_t)file_size, file);
    buffer[bytes_read] = '\0';
    fclose(file);

    send_http_response(client_socket, 200, content_type, buffer);
    free(buffer);
}

static void handle_static_request(int client_socket, const char *path) {
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        send_file_response(client_socket, WEB_ROOT "/index.html", "text/html; charset=utf-8");
        return;
    }

    if (strcmp(path, "/app.js") == 0) {
        send_file_response(client_socket, WEB_ROOT "/app.js", "application/javascript; charset=utf-8");
        return;
    }

    if (strcmp(path, "/style.css") == 0) {
        send_file_response(client_socket, WEB_ROOT "/style.css", "text/css; charset=utf-8");
        return;
    }

    if (strncmp(path, "/web/", 5) == 0) {
        const char *asset_path = path + 1;
        if (strstr(asset_path, "..") != NULL) {
            send_http_response(client_socket, 400, "text/plain", "Bad Request");
            return;
        }

        const char *content_type = "text/plain; charset=utf-8";
        const char *extension = strrchr(asset_path, '.');
        if (extension && strcmp(extension, ".js") == 0) {
            content_type = "application/javascript; charset=utf-8";
        } else if (extension && strcmp(extension, ".css") == 0) {
            content_type = "text/css; charset=utf-8";
        } else if (extension && strcmp(extension, ".html") == 0) {
            content_type = "text/html; charset=utf-8";
        }

        send_file_response(client_socket, asset_path, content_type);
        return;
    }

    send_http_response(client_socket, 404, "text/plain", "Not Found");
}

/**
 * Handle HTTP GET /api/status
 */
static void handle_status_request(int client_socket) {
    char *json = export_heap_state_json(g_allocator);
    if (json) {
        send_http_response(client_socket, 200, "application/json", json);
        free_json_state(json);
    } else {
        send_http_response(client_socket, 500, "text/plain", "ERROR: Failed to export heap state");
    }
}

/**
 * Handle HTTP POST /api/malloc
 */
static void handle_malloc_request(int client_socket, const char *body) {
    size_t size = 0;
    sscanf(body, "{\"size\": %zu}", &size);

    if (size <= 0 || size > HEAP_SIZE) {
        send_http_response(client_socket, 400, "application/json",
                          "{\"error\": \"Invalid size\"}");
        return;
    }

    void *ptr = allocator_malloc(g_allocator, size);

    if (ptr == NULL) {
    fprintf(stderr, "Allocation failed: Out of Memory\n");
    // Send an HTTP 500 error back to the browser instead of crashing
    send_http_response(client_socket, 500, "text/plain", "Internal Server Error: Out of Memory");
    }

// Now it is safe to use ptr
    // memcpy(ptr, incoming_data, requested_size);

    char response[512];
    if (ptr) {
        snprintf(response, sizeof(response),
                "{\"success\": true, \"address\": \"%p\", \"size\": %zu}",
                ptr, size);
        send_http_response(client_socket, 200, "application/json", response);
    } else {
        snprintf(response, sizeof(response),
                "{\"success\": false, \"error\": \"Allocation failed\"}");
        send_http_response(client_socket, 200, "application/json", response);
    }
}

/**
 * Handle HTTP POST /api/free
 */
static void handle_free_request(int client_socket, const char *body) {
    void *ptr = NULL;
    sscanf(body, "{\"address\": \"%p\"}", &ptr);

    if (!ptr) {
        send_http_response(client_socket, 400, "application/json",
                          "{\"error\": \"Invalid address\"}");
        return;
    }

    allocator_free(g_allocator, ptr);

    send_http_response(client_socket, 200, "application/json",
                      "{\"success\": true}");
}

/**
 * Handle HTTP POST /api/reset
 */
static void handle_reset_request(int client_socket) {
    allocator_reset(g_allocator);
    send_http_response(client_socket, 200, "application/json",
                      "{\"success\": true, \"message\": \"Heap reset\"}");
}

/**
 * Handle HTTP POST /api/verify
 */
static void handle_verify_request(int client_socket) {
    bool valid = allocator_verify(g_allocator);
    char response[256];
    snprintf(response, sizeof(response),
            "{\"valid\": %s}",
            valid ? "true" : "false");
    send_http_response(client_socket, 200, "application/json", response);
}

/**
 * Parse HTTP request and route to handler
 */
static void handle_http_request(int client_socket, const char *request) {
    char method[16] = {0};
    char path[256] = {0};
    char *body = NULL;

    /* Parse request line */
    sscanf(request, "%15s %255s", method, path);

    /* Find body separator */
    char *body_start = strstr(request, "\r\n\r\n");
    if (body_start) {
        body = body_start + 4;
    }

    /* Route request */
    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/api/status") == 0) {
            handle_status_request(client_socket);
        } else {
            handle_static_request(client_socket, path);
        }
    } else if (strcmp(method, "POST") == 0) {
        if (strcmp(path, "/api/malloc") == 0) {
            handle_malloc_request(client_socket, body ? body : "");
        } else if (strcmp(path, "/api/free") == 0) {
            handle_free_request(client_socket, body ? body : "");
        } else if (strcmp(path, "/api/reset") == 0) {
            handle_reset_request(client_socket);
        } else if (strcmp(path, "/api/verify") == 0) {
            handle_verify_request(client_socket);
        } else {
            send_http_response(client_socket, 404, "text/plain", "Not Found");
        }
    } else if (strcmp(method, "OPTIONS") == 0) {
        send_http_response(client_socket, 200, "text/plain", "");
    } else {
        send_http_response(client_socket, 400, "text/plain", "Bad Request");
    }
}

/**
 * Client handler thread
 */
static void *client_handler_thread(void *arg) {
    ClientInfo *client_info = (ClientInfo *)arg;
    char buffer[BUFFER_SIZE] = {0};

    int bytes_received = recv(client_info->client_socket, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        handle_http_request(client_info->client_socket, buffer);
    }

    close(client_info->client_socket);
    free(client_info);

    return NULL;
}

/**
 * Server main loop
 */
static void server_main_loop(void) {
    printf("Memory Allocator Server listening on port %d...\n", SERVER_PORT);
    printf("Access the GUI at http://localhost:%d\n", SERVER_PORT);

    while (g_running) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_socket = accept(g_server_socket,
                                  (struct sockaddr *)&client_addr,
                                  &client_addr_len);

        if (client_socket < 0) {
            if (g_running) {
                perror("accept");
            }
            continue;
        }

        /* Create thread for client */
        ClientInfo *client_info = malloc(sizeof(ClientInfo));
        if (!client_info) {
            close(client_socket);
            continue;
        }

        client_info->client_socket = client_socket;
        client_info->client_addr = client_addr;

        pthread_t thread;
        if (pthread_create(&thread, NULL, client_handler_thread, client_info) != 0) {
            fprintf(stderr, "Failed to create client thread\n");
            free(client_info);
            close(client_socket);
        } else {
            pthread_detach(thread);
        }
    }
}

/**
 * Signal handler
 */
static void signal_handler(int sig) {
    printf("\nShutting down server...\n");
    g_running = false;
    if (g_server_socket >= 0) {
        close(g_server_socket);
    }
}

/**
 * Initialize server socket
 */
static int initialize_server(void) {
    g_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_socket < 0) {
        perror("socket");
        return -1;
    }

    /* Allow socket reuse */
    int opt = 1;
    if (setsockopt(g_server_socket, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(g_server_socket);
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SERVER_PORT);

    if (bind(g_server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(g_server_socket);
        return -1;
    }

    if (listen(g_server_socket, LISTEN_BACKLOG) < 0) {
        perror("listen");
        close(g_server_socket);
        return -1;
    }

    return 0;
}

/**
 * Main entry point
 */
int main(int argc, char *argv[]) {
    printf("╔════════════════════════════════════════════╗\n");
    printf("║  Custom Memory Allocator - REST API Server ║\n");
    printf("║            Thread-Safe (Mutex)             ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    /* Initialize allocator */
    g_allocator = allocator_init();
    if (!g_allocator) {
        fprintf(stderr, "ERROR: Failed to initialize allocator\n");
        return EXIT_FAILURE;
    }

    printf("✓ Allocator initialized (4KB heap, thread-safe)\n");

    /* Initialize server */
    if (initialize_server() < 0) {
        fprintf(stderr, "ERROR: Failed to initialize server\n");
        allocator_destroy(g_allocator);
        return EXIT_FAILURE;
    }

    printf("✓ Server initialized\n\n");

    /* Setup signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Run server */
    server_main_loop();

    /* Cleanup */
    printf("Cleaning up...\n");
    allocator_destroy(g_allocator);

    printf("✓ Shutdown complete\n");

    return EXIT_SUCCESS;
}
