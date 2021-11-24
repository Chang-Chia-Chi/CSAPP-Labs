#include <stdio.h>
#include "csapp.h"

/* FIFO thread-safe queue */
typedef struct {
    int *buf;     /* buffer */
    int maxnum;   /* max number of items in buffer */
    int front;    /* (front + 1) = first item in buffer */
    int rear;     /* rear = last item in buffer */

    sem_t insert;
    sem_t remove;
    sem_t mutex;
} queue_t;

void do_request(int fd);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void build_header(char *header, char *hostname, char *path, int port, rio_t *myio);
int connect_server(char *hostname, int port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg);

void queue_init(int n);
void queue_free(queue_t *queue);
void queue_put(queue_t *queue, int item);
int queue_get(queue_t *queue);
void *thread_fn(void *vargp);

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* Threads number and Queue size */
#define NUM_THREADS 4
#define QUEUE_SIZE 16

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *proxy_conn_hdr = "Proxy-Connection: close\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *end_hdr = "\r\n";
queue_t *buf_queue;

int main(int argc, char **argv)
{   
    int listen_fd, conn_fd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /* get listen fd */
    int ret, optval=1;
    struct addrinfo hints, *list_ptr;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;             /* Accept connections */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* ... on any IP address */
    hints.ai_flags |= AI_NUMERICSERV;            /* ... using port number */
    
    if ((ret=getaddrinfo(NULL, argv[1], &hints, &list_ptr)) != 0) {
        fprintf(stderr, "getaddrinfo failed (port %s): %s\n", argv[1], gai_strerror(ret));
        exit(1);
    }

    struct addrinfo *ptr;
    for (ptr = list_ptr; ptr; ptr=ptr->ai_next) {
        if ((listen_fd=socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) < 0)
            continue;

        /* Eliminates "Address already in use" error from bind */
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval , sizeof(int));
        
        if (bind(listen_fd, ptr->ai_addr, ptr->ai_addrlen) == 0)
            break; /* Success create server listen socker */
        
        /* close fd created to prevent fd leakage */
        close(listen_fd);
    }
    freeaddrinfo(list_ptr);
    if (!ptr) exit(1);
    if (listen(listen_fd, LISTENQ) < 0) {
        close(listen_fd);
        fprintf(stderr, "listen failed (port %s)\n", argv[1]);
        exit(1);
    }

    queue_init(QUEUE_SIZE);
    pthread_t tid[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        ret = pthread_create(&tid[i], NULL, thread_fn, NULL);
        if (ret != 0)
            fprintf(stderr, "pthread_create failed, idx %d\n", i);
    }

    while (1) {
        clientlen = sizeof(clientaddr);
        conn_fd = accept(listen_fd, (SA *)&clientaddr, &clientlen);
        if ((ret=getnameinfo((SA *)&clientaddr, clientlen, hostname, 
                             MAXLINE, port, MAXLINE, 0)) != 0) {
            
            fprintf(stderr, "getnameinfo failed, %s\n", gai_strerror(ret));
            continue;
        }
        printf("Accept connection from (%s, %s)\n", hostname, port);
        queue_put(buf_queue, conn_fd);
    }
    queue_free(buf_queue);
    return 0;
}

int connect_server(char *hostname, int port) {
    int connfd, ret;
    struct addrinfo hints, *list_ptr;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;             /* Accept connections */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* ... on any IP address */
    hints.ai_flags |= AI_NUMERICSERV;            /* ... using port number */
    
    char port_str[10];
    sprintf(port_str, "%d", port);
    if ((ret=getaddrinfo(hostname, port_str, &hints, &list_ptr)) != 0) {
        fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", hostname, port_str, gai_strerror(ret));
        return -2;
    }

    struct addrinfo *ptr;
    for (ptr = list_ptr; ptr; ptr=ptr->ai_next) {
        if ((connfd=socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) < 0)
            continue; /* Failed */
        if ((connect(connfd, ptr->ai_addr, ptr->ai_addrlen)) != -1)
            break; /* Success */
        close(connfd);
    }

    freeaddrinfo(list_ptr);
    if (!ptr) return -1;
    return connfd;
}

/* Implementation of functions for proxy */
void do_request(int clientfd) {
    int n_read;
    int port = 80;
    char header_to_server[MAXLINE];
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE];
    rio_t client_myio;
    Rio_readinitb(&client_myio, clientfd);
    n_read = Rio_readlineb(&client_myio, buf, MAXLINE);
    if (!n_read)
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) {
        clienterror(clientfd, method, "501", "Method Not Implemented");
        return;
    }
    parse_uri(buf, hostname, path, &port);
    build_header(header_to_server, hostname, path, port, &client_myio);

    int serverfd;
    rio_t server_myio;
    serverfd = connect_server(hostname, port);
    if (serverfd < 0) {
        fprintf(stderr, "connect_server failed\n");
        return;
    }

    /* Read response from server and send to client */
    Rio_readinitb(&server_myio, serverfd);
    /* Send request to server */
    Rio_writen(serverfd, header_to_server, strlen(header_to_server));
    while ((n_read = Rio_readlineb(&server_myio, buf, MAXLINE)) > 0) {
        printf("Proxy receives %d bytes from server\n", n_read);
        Rio_writen(clientfd, buf, n_read);
    }
    close(serverfd);
}

void parse_uri(char *uri, char *hostname, char *path, int *port) {
    char *ptr, *host_ptr, *path_ptr, *port_ptr;
    ptr = strstr(uri, "//");
    ptr = ptr? ptr+2:uri;
    port_ptr = strstr(ptr, ":");
    if (!port_ptr) /* no specific port number */
    {
        host_ptr = strstr(ptr, "/");
        if (!host_ptr)
            sscanf(ptr, "%s", hostname);
        else {
            *host_ptr = '\0';
            sscanf(ptr, "%s", hostname);
            path_ptr = host_ptr;
            *path_ptr = '/';
            sscanf(path_ptr, "%s", path);
        }
    }
    else /* web browser indicate port numer */
    {
        *port_ptr = '\0';
        sscanf(ptr, "%s", hostname);
        port_ptr += 1;
        sscanf(port_ptr, "%d%s", port, path);
    }
}

void build_header(char *header, char* hostname, char* path, int port, rio_t *myio) {

    char buf[MAXLINE], request_hrd[MAXLINE], host_hdr[MAXLINE], other_hdr[MAXLINE];

    sprintf(request_hrd, "GET %s HTTP/1.0\r\n", path);
    sprintf(host_hdr, "HOST: %s\r\n", hostname);
    while (Rio_readlineb(myio, buf, MAXLINE) > 0) {
        if (!strcmp(buf, end_hdr)) /* EOF */
            break;
        else if (!(strncasecmp(buf, "HOST", 4))) { /* HOST: */
            strcpy(host_hdr, buf);
        }
        else if (!(strncasecmp(buf, "User-Agent", 10)) ||
                 !(strncasecmp(buf, "Proxy-Connection", 16)) ||
                 !(strncasecmp(buf, "Connection", 10))) /* headers should not be changed */
            continue;
        else
            strcat(other_hdr, buf);
    }

    sprintf(header, "%s%s%s%s%s%s%s",
            request_hrd,
            host_hdr,
            conn_hdr,
            proxy_conn_hdr,
            user_agent_hdr,
            other_hdr,
            end_hdr);
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Proxy Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s %s: %s\r\n", cause, errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
}

void queue_init(int n) {
    buf_queue = malloc(sizeof(queue_t));
    if (!buf_queue) {
        fprintf(stderr, "queue_init failed to allocate memory for queue_t\n");
        exit(1);
    }
    buf_queue->buf = calloc(0, sizeof(int) * n);
    if (!buf_queue->buf) {
        fprintf(stderr, "queue_init failed to allocate memory for buffer\n");
        exit(1);
    }
    buf_queue->maxnum = n;
    buf_queue->front = 0;
    buf_queue->rear = 0;
    sem_init(&buf_queue->mutex, 0, 1);
    sem_init(&buf_queue->insert, 0, n);
    sem_init(&buf_queue->remove, 0, 0);
}

void queue_free(queue_t *queue) {
    free(queue->buf);
    free(queue);
}

void queue_put(queue_t *queue, int item) {
    sem_wait(&queue->insert);
    sem_wait(&queue->mutex);
    queue->buf[(++queue->rear) % (queue->maxnum)] = item;
    sem_post(&queue->mutex);
    sem_post(&queue->remove);
}

int queue_get(queue_t *queue) {
    int item;

    sem_wait(&queue->remove);
    sem_wait(&queue->mutex);
    item = queue->buf[(++queue->front) % (queue->maxnum)];
    sem_post(&queue->mutex);
    sem_post(&queue->insert);
    return item;
}

void *thread_fn(void *vargp) {
    int ret, conn_fd;
    
    ret = pthread_detach(pthread_self());
    if (ret != 0)
        fprintf(stderr, "pthread_detach failed, tid: %ld\n", pthread_self());
    
    while (1) {
        conn_fd = queue_get(buf_queue);
        do_request(conn_fd);
        close(conn_fd);
    }
}