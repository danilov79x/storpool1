#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "unique_count.h"

static int write_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

static int run_case(const uint32_t *values, size_t count, uint64_t expect_unique, uint64_t expect_once) {
    char path[] = "/tmp/unique_count_testXXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        perror("mkstemp");
        return 1;
    }

    int rc = 0;
    size_t bytes = count * sizeof(uint32_t);
    if (bytes > 0 && write_all(fd, values, bytes) != 0) {
        perror("write");
        rc = 1;
        goto out;
    }

    if (close(fd) != 0) {
        perror("close");
        rc = 1;
        fd = -1;
        goto out;
    }
    fd = -1;

    CountResult result;
    if (count_file(path, &result, stderr) != 0) {
        fprintf(stderr, "count_file failed for %s\n", path);
        rc = 1;
        goto out;
    }

    if (result.unique_count != expect_unique || result.seen_once_count != expect_once) {
        fprintf(
            stderr,
            "unexpected result: unique=%llu once=%llu (expected unique=%llu once=%llu)\n",
            (unsigned long long)result.unique_count,
            (unsigned long long)result.seen_once_count,
            (unsigned long long)expect_unique,
            (unsigned long long)expect_once
        );
        rc = 1;
    }

out:
    if (fd >= 0) {
        close(fd);
    }
    unlink(path);
    return rc;
}

static int run_invalid_size_case(void) {
    char path[] = "/tmp/unique_count_invalidXXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        perror("mkstemp");
        return 1;
    }

    const uint8_t bad[3] = {1u, 2u, 3u};
    int rc = 0;
    if (write_all(fd, bad, sizeof(bad)) != 0) {
        perror("write");
        rc = 1;
        goto out;
    }

    if (close(fd) != 0) {
        perror("close");
        rc = 1;
        fd = -1;
        goto out;
    }
    fd = -1;

    CountResult result = {0u, 0u};
    if (count_file(path, &result, NULL) == 0) {
        fprintf(stderr, "expected invalid-size input to fail\n");
        rc = 1;
    }

out:
    if (fd >= 0) {
        close(fd);
    }
    unlink(path);
    return rc;
}

int main(void) {
    const uint32_t case1[] = {0x100u, 0x100u, 0xfffu, 0xfffu};
    const uint32_t case2[] = {0x100u, 0x100u, 0x100u, 0x100u};
    const uint32_t case3[] = {0x100u, 0x100u, 0x800u, 0xfffu};
    const uint32_t case5[] = {0u, 0xFFFFFFFFu, 0u, 0x7FFFFFFFu, 0xFFFFFFFFu};

    int failed = 0;
    failed |= run_case(case1, sizeof(case1) / sizeof(case1[0]), 2u, 0u);
    failed |= run_case(case2, sizeof(case2) / sizeof(case2[0]), 1u, 0u);
    failed |= run_case(case3, sizeof(case3) / sizeof(case3[0]), 3u, 2u);
    failed |= run_case(NULL, 0u, 0u, 0u);
    failed |= run_case(case5, sizeof(case5) / sizeof(case5[0]), 3u, 1u);
    failed |= run_invalid_size_case();

    if (failed != 0) {
        return 1;
    }

    printf("all tests passed\n");
    return 0;
}
