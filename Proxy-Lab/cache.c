#include "cache.h"

void cache_init(cache_t *caches, int nblk) {
    for (int i = 0; i < nblk; i++) {
        caches[i].valid = 0;
        caches[i].stamp = 0;
        caches[i].bufsize = 0;
        caches[i].readcnt = 0;
        caches[i].writecnt = 0;

        sem_init(&caches[i].ready, 0, 1);
        sem_init(&caches[i].reader, 0, 1);
        sem_init(&caches[i].writer, 0, 1);
    }
}

int cache_srch(cache_t *caches, char *uri, int connfd) {
    int cached = 0;
    int to_write = 0;
    for (int i = 0; i < NUM_CACHE_BLK; i++) {
        sem_wait(&caches[i].ready);
        sem_wait(&caches[i].reader);
        caches[i].readcnt++;
        if (caches[i].readcnt == 1)
            sem_wait(&caches[i].writer);
        if (caches[i].valid && !strcmp(caches[i].uri, uri)) {
            cached = 1;
            to_write = 1;
            caches[i].stamp = 0;
        }
        else caches[i].stamp++;
        
        if (cached && to_write) {
            Rio_writen(connfd, caches[i].buf, caches[i].bufsize);
            to_write = 0;
        }
        
        caches[i].readcnt--;
        if (caches[i].readcnt == 0)
            sem_post(&caches[i].writer);
        sem_post(&caches[i].reader);
        sem_post(&caches[i].ready);
    }
    return cached;
}

void cache_evict(cache_t *caches, char *uri, char *buf, size_t nbytes) {
    int max_stamp = INT_MIN;
    int idx;
    for (int i = 0; i < NUM_CACHE_BLK; i++) {
        if (!caches[i].valid) {
            idx = i;
            break;   
        }

        if (caches[i].stamp > max_stamp) {
            idx = i;
            max_stamp = caches[i].stamp;
        }
    }

    sem_wait(&caches[idx].writer);
    caches[idx].writecnt++;
    if (caches[idx].writecnt == 1) 
        sem_wait(&caches[idx].ready);
    caches[idx].valid = 1;
    caches[idx].stamp = 0;
    caches[idx].bufsize = nbytes;
    strcpy(caches[idx].uri, uri);
    strcpy(caches[idx].buf, buf);
    caches[idx].writecnt--;
    if (caches[idx].writecnt == 0) 
        sem_post(&caches[idx].ready);
    sem_post(&caches[idx].writer);
    return;
}