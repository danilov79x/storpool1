#ifndef UNIQUE_COUNT_H
#define UNIQUE_COUNT_H

#include <stdint.h>
#include <stdio.h>

typedef struct CountResult {
    uint64_t unique_count;
    uint64_t seen_once_count;
} CountResult;

int count_file(const char *path, CountResult *out, FILE *err_stream);

#endif
