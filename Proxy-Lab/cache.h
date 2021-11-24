#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define NUM_CACHE_BLK 10

typedef struct {
    int valid;
    int stamp;
    int readcnt;
    int writecnt;
    size_t bufsize;
    char uri[MAXLINE];
    char buf[MAX_OBJECT_SIZE];

    sem_t ready;
    sem_t reader;
    sem_t writer;
} cache_t;

extern cache_t cache_blks[NUM_CACHE_BLK];

void cache_init();
void cache_free();
void cache_evict(cache_t *caches, char *uri, char *buf, size_t nbytes);
int cache_srch(cache_t *caches, char *uri, int connfd);