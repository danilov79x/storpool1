#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "unique_count.h"

#define CHUNK_INTS (UINT64_C(1) << 20)     /* 1,048,576 uint32_t values per read */
#define CHUNK_BYTES (CHUNK_INTS * sizeof(uint32_t))
#define PAGE_COUNT (1u << 16)              /* One page per high 16 bits */
#define PAGE_BYTES (1u << 13)              /* 2^16 bits/page = 8 KiB */

static void fail_errno(FILE *err_stream, const char *msg) {
    if (err_stream != NULL) {
        fprintf(err_stream, "%s: %s\n", msg, strerror(errno));
    }
}

static void free_pages(uint8_t **pages) {
    if (pages == NULL) {
        return;
    }
    for (size_t i = 0; i < PAGE_COUNT; ++i) {
        free(pages[i]);
    }
    free(pages);
}

static uint8_t *get_or_create_page(uint8_t **pages, uint32_t page_index) {
    uint8_t *page = pages[page_index];
    if (page != NULL) {
        return page;
    }

    page = (uint8_t *)calloc(PAGE_BYTES, 1);
    if (page == NULL) {
        return NULL;
    }
    pages[page_index] = page;
    return page;
}

int count_file(const char *path, CountResult *out, FILE *err_stream) {
    int fd = -1;
    uint32_t *buffer = NULL;
    uint8_t **pages_once = NULL;
    uint8_t **pages_multi = NULL;
    int rc = -1;

    if (path == NULL || out == NULL) {
        errno = EINVAL;
        fail_errno(err_stream, "invalid argument");
        return -1;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        fail_errno(err_stream, "open failed");
        goto cleanup;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        fail_errno(err_stream, "fstat failed");
        goto cleanup;
    }

    if ((st.st_size % (off_t)sizeof(uint32_t)) != 0) {
        if (err_stream != NULL) {
            fprintf(err_stream, "Input size (%jd bytes) is not a multiple of 4.\n", (intmax_t)st.st_size);
        }
        errno = EINVAL;
        goto cleanup;
    }

    pages_once = (uint8_t **)calloc(PAGE_COUNT, sizeof(*pages_once));
    if (pages_once == NULL) {
        fail_errno(err_stream, "calloc failed for page table (once)");
        goto cleanup;
    }

    pages_multi = (uint8_t **)calloc(PAGE_COUNT, sizeof(*pages_multi));
    if (pages_multi == NULL) {
        fail_errno(err_stream, "calloc failed for page table (multi)");
        goto cleanup;
    }

    buffer = (uint32_t *)malloc(CHUNK_BYTES);
    if (buffer == NULL) {
        fail_errno(err_stream, "malloc failed for read buffer");
        goto cleanup;
    }

    uint64_t unique_count = 0;
    uint64_t seen_once_count = 0;
    uint64_t processed_values = 0;
    uint64_t total_values = (uint64_t)st.st_size / sizeof(uint32_t);
    time_t next_report_time = time(NULL) + 5;

    for (;;) {
        ssize_t n = read(fd, buffer, CHUNK_BYTES);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            fail_errno(err_stream, "read failed");
            goto cleanup;
        }

        if (n == 0) {
            break;
        }

        if ((n % (ssize_t)sizeof(uint32_t)) != 0) {
            if (err_stream != NULL) {
                fprintf(err_stream, "Read returned a non-multiple of 4 bytes (%zd).\n", n);
            }
            errno = EIO;
            goto cleanup;
        }

        size_t cnt = (size_t)n / sizeof(uint32_t);
        processed_values += (uint64_t)cnt;
        for (size_t i = 0; i < cnt; ++i) {
            uint32_t value = buffer[i];
            uint32_t page_index = value >> 16;
            uint32_t low = value & 0xFFFFu;
            uint32_t byte_index = low >> 3;
            uint8_t bit_mask = (uint8_t)(1u << (low & 7u));
            uint8_t *page_multi = pages_multi[page_index];
            if (page_multi != NULL && (page_multi[byte_index] & bit_mask) != 0) {
                continue;
            }

            uint8_t *page_once = pages_once[page_index];
            if (page_once == NULL) {
                page_once = get_or_create_page(pages_once, page_index);
                if (page_once == NULL) {
                    fail_errno(err_stream, "calloc failed for bitset page (once)");
                    goto cleanup;
                }
            }

            if ((page_once[byte_index] & bit_mask) == 0) {
                page_once[byte_index] |= bit_mask;
                ++unique_count;
                ++seen_once_count;
            } else {
                page_once[byte_index] &= (uint8_t)~bit_mask;
                --seen_once_count;

                if (page_multi == NULL) {
                    page_multi = get_or_create_page(pages_multi, page_index);
                    if (page_multi == NULL) {
                        fail_errno(err_stream, "calloc failed for bitset page (multi)");
                        goto cleanup;
                    }
                }
                page_multi[byte_index] |= bit_mask;
            }
        }

        if (err_stream != NULL) {
            time_t now = time(NULL);
            if (now >= next_report_time) {
                double percent = 100.0;
                if (total_values > 0) {
                    percent = ((double)processed_values * 100.0) / (double)total_values;
                }
                fprintf(err_stream,
                        "progress: processed=%" PRIu64 "/%" PRIu64 " (%.2f%%) unique=%" PRIu64 " seen_once=%" PRIu64 "\n",
                        processed_values,
                        total_values,
                        percent,
                        unique_count,
                        seen_once_count);
                next_report_time = now + 5;
            }
        }
    }

    out->unique_count = unique_count;
    out->seen_once_count = seen_once_count;
    rc = 0;

cleanup:
    free(buffer);
    free_pages(pages_multi);
    free_pages(pages_once);
    if (fd >= 0) {
        close(fd);
    }
    return rc;
}

#ifndef UNIQUE_COUNT_NO_MAIN
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <binary_file>\n", argv[0]);
        return 1;
    }

    CountResult result;
    if (count_file(argv[1], &result, stderr) != 0) {
        return 1;
    }

    printf("unique=%" PRIu64 " seen_once=%" PRIu64 "\n", result.unique_count, result.seen_once_count);
    return 0;
}
#endif
