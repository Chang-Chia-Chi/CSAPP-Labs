#include "cachelab.h"
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAX_COUNT 100 /* maximum number of splited string array could handle by split function */
#define MAX_STR_LEN 256

typedef unsigned long int uint64_t;
typedef struct {
    int valid;
    int stamp;
    uint64_t tag;
} cache_line;
typedef cache_line* associate;

/* global variable */
associate* cache_track;
int hits = 0, misses = 0, evictions = 0;
int verbos = 0, set_bits = 0, set_num_lines = 0, block_bits = 0;
char trace_filename[50]; /* create file name buffer to ensure initial memory allocation */

int read_argv(int argc, char* argv[]){
    
    /* # of args less than expected */
    if (argc < 5) return 0;

    char* endptr;
    char* file_ext;
    int cmd_opt, flag_check[4];
    while (1) {
        cmd_opt = getopt(argc, argv, "vs:E:b:t:");

        /* has permuted all argv */
        if (cmd_opt == -1) break;

        switch (cmd_opt) {
            case 's':
                set_bits = strtol(optarg, &endptr, 10);
                flag_check[0] = 1;
                break;
            case 'E':
                set_num_lines = strtol(optarg, &endptr, 10);
                flag_check[1] = 1;
                break;
            case 'b':
                block_bits = strtol(optarg, &endptr, 10);
                flag_check[2] = 1;
                break;
            case 't':
                strncpy(trace_filename, optarg, MAX_STR_LEN);
                trace_filename[MAX_STR_LEN - 1] = '\0';
                file_ext = strchr(trace_filename, '.');
                /* if '.' is not in file name or extension is in wrong format*/
                if (file_ext == NULL || strcmp(file_ext, ".trace") != 0) {
                    printf("trace file should be with extension .trace\n");
                    return 0;
                }
                flag_check[3] = 1;
                break;
            case 'v':
                verbos = 1;
                break;
            case '?': /* unexpected flag or miss argument for option*/
                return 0;
        }
    }

    /* check whether arguments of -s -E -b -t are all correctly entered*/
    for (int i = 0; i < 4; i++) {
        if (flag_check[i] == 0) return 0;
    }
    return 1;
}

int split_add(char* dest_arr[], const char* src_str, int dest_idx, int left, int right){
    if (dest_idx >= MAX_COUNT) {
        printf("index out of bound\n");
        return -1;
    }

    int idx = 0;
    char str_buf[MAX_STR_LEN];
    for (int i = left; i < right; i++){
        str_buf[idx] = src_str[i];
        idx++;
        if (idx >= MAX_STR_LEN) break;
    }
    str_buf[idx] = '\0';
    strncpy(dest_arr[dest_idx], str_buf, MAX_STR_LEN);

    return 0;
}

int split(char* dest_arr[], const char* src_str, const char split_ch){
    int str_len = strlen(src_str);
    int max_count = MAX_COUNT;
    int i = 0, j = 0;
    while (max_count-- > 0) {
        while (i < str_len && src_str[i] == split_ch)
            i++;

        if (i == str_len) break; /* reach end of string */

        j = i; i++;
        while (i < str_len && src_str[i] != split_ch && src_str[i] != '\n')
            i++;
        split_add(dest_arr, src_str, MAX_COUNT - max_count - 1, j, i);
    }
    return 0;
}

int initial_associate(){
    int num_sets = 1 << set_bits;
    cache_track = malloc(sizeof(associate) * num_sets);
    if (!cache_track) return 0;

    for (int i = 0; i < num_sets; i++){
        cache_track[i]= malloc(sizeof(cache_line) * set_num_lines);
        if (!cache_track[i]) return 0;
        for (int j = 0; j < set_num_lines; j++){
            cache_track[i]->stamp = 0;
            cache_track[i]->tag = 0;
            cache_track[i]->valid = 0;
        }
    }
    return 1;
}

int free_associate(){
    int num_sets = 1 << set_bits;
    for (int i = 0; i < num_sets; i++){
        free(cache_track[i]);
    }
    free(cache_track);
    return 0;
}

int checkcache(uint64_t address){
    uint64_t set_idx = (address >> block_bits) & ((1 << set_bits) - 1);
    uint64_t tag_val = address >> (set_bits + block_bits);

    int max_timestamp = INT_MIN;
    int empty_line = -1;
    int evic_idx = 0;
    associate cache_set = cache_track[set_idx];
    for (int i = 0; i < set_num_lines; i++){
        if (cache_set[i].valid) {
            if (cache_set[i].tag == tag_val){
                hits++;
                cache_set[i].stamp = 0;
                return 0;
            }
            cache_set[i].stamp++;

            /* get new index to evict */
            /* = is necessary to get all credits that last line will be evicted if tie */
            /* to ensure this counter has the identical behavior as queue */
            if (cache_set[i].stamp >= max_timestamp){
                evic_idx = i;
                max_timestamp = cache_set[i].stamp;
            }
        }
        else {
            empty_line = i;
        }
    }

    misses++;
    if (empty_line != -1){ /* update associate array to empty line */
        cache_set[empty_line].valid = 1;
        cache_set[empty_line].stamp = 0;
        cache_set[empty_line].tag = tag_val;
        return 1;
    } else { /* update associate array if eviction happen */
        evictions++;
        cache_set[evic_idx].valid = 1;
        cache_set[evic_idx].stamp = 0;
        cache_set[evic_idx].tag = tag_val;
        return 2;
    }
}

int main(int argc, char* argv[])
{   
    /********************/
    /*   intialization  */
    /********************/
    /* read argv */
    int status;
    status = read_argv(argc, argv);

    /* failed to read arguments */
    if (!status) {
        fprintf(stderr, "Usage: ./csim [-v] -s <s> -E <E> -b <b> -t <tracefile>\n");
        exit(0);
    }

    /* open trace file */
    FILE* trace_file = fopen(trace_filename, "r");
    if (!trace_file) {
        fprintf(stderr, "Failed to open trace file: %s\n", trace_filename);
        exit(0);
    }

    /* initialize a array to track cache */
    status = initial_associate();
    if (!status) {
        fprintf(stderr, "Failed to allocate memory for cache tracking array\n");
        exit(0);
    }

    /********************/
    /* parse trace file */
    /********************/
    /* allocate space for sub string array */
    char* split_str[MAX_COUNT];
    char* addr_size[MAX_COUNT];
    for (int i = 0; i < MAX_COUNT; i++){
        split_str[i] = malloc(sizeof(char) * MAX_STR_LEN);
    }
    for (int i = 0; i < MAX_COUNT; i++){
        addr_size[i] = malloc(sizeof(char) * MAX_STR_LEN);
    }

    /* read trace file line by line */
    uint64_t address;
    int result;
    char line[MAX_STR_LEN];
    char type_ch, *op_type, *data_addr, *op_size,*endptr;
    while (fgets(line, MAX_STR_LEN, trace_file)){
        if (line[0] == 'I') continue; /* Instruction */

        /* split line to get operand type, address of data & size of operand data */
        /* just for practice purpose of implement split function */
        /* could be simplified by using sscanf(line, " %c %lx,%d", &op_type, &data_addr, &op_size); */
        split(split_str, line, ' ');
        op_type = split_str[0];
        split(addr_size, split_str[1], ',');
        data_addr = addr_size[0];
        op_size = addr_size[1];

        /* compute cache condition of each data manipulation */
        address = strtoul(data_addr, &endptr, 16);
        result = checkcache(address);

        type_ch = op_type[0];
        if (type_ch == 'M') hits++;
        
        /* print out parse result if -v flag indicated */
        if (verbos){
            switch (result){
                case 0:
                    printf("%s %s,%s hit\n", op_type, data_addr, op_size);
                    break;
                case 1:
                    if (type_ch == 'M'){
                        printf("%s %s,%s miss hit\n", op_type, data_addr, op_size);
                    } else {
                        printf("%s %s,%s miss\n", op_type, data_addr, op_size);
                    }
                    
                    break;
                case 2:
                    if (type_ch == 'M'){
                        printf("%s %s,%s miss eviction hit\n", op_type, data_addr, op_size);
                    } else {
                        printf("%s %s,%s miss eviction\n", op_type, data_addr, op_size);
                    }
                    
                    break;
            }
        }
    }

    /* free memory and close file */
    for (int i = 0; i < MAX_COUNT; i++){
        free(split_str[i]);
        free(addr_size[i]);
    }
    free_associate();
    fclose(trace_file);
    printSummary(hits, misses, evictions);
    return 0;
}