#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COUNT UINT64_C(1000000000)
#define CHUNK_INTS (1u << 20)

typedef enum Mode {
    MODE_RANDOM,
    MODE_SEQUENTIAL,
    MODE_CONSTANT,
    MODE_GAUSSIAN
} Mode;

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s -o <file> -n <count> [options]\n"
            "\n"
            "Generate binary file of uint32_t values.\n"
            "\n"
            "Required:\n"
            "  -o, --output <path>      Output binary file\n"
            "  -n, --count <N>          Number of uint32_t values (0..1000000000)\n"
            "\n"
            "Options:\n"
            "  -m, --mode <name>        random | sequential | constant | gaussian (default: random)\n"
            "  -s, --seed <S>           Seed for random mode (default: 1)\n"
            "      --start <V>          Start value for sequential mode (default: 0)\n"
            "      --value <V>          Value for constant mode (default: 0)\n"
            "      --mean <M>           Mean for gaussian mode (default: 2147483648.0)\n"
            "      --stddev <D>         Stddev for gaussian mode, must be > 0 (default: 1000000.0)\n"
            "  -h, --help               Show this help\n",
            prog);
}

static int parse_u64(const char *s, uint64_t *out) {
    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 0);
    if (errno != 0 || end == s || *end != '\0') {
        return -1;
    }
    *out = (uint64_t)v;
    return 0;
}

static int parse_u32(const char *s, uint32_t *out) {
    uint64_t v;
    if (parse_u64(s, &v) != 0 || v > UINT32_MAX) {
        return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

static int parse_f64(const char *s, double *out) {
    char *end = NULL;
    errno = 0;
    double v = strtod(s, &end);
    if (errno != 0 || end == s || *end != '\0') {
        return -1;
    }
    *out = v;
    return 0;
}

static int parse_mode(const char *s, Mode *mode) {
    if (strcmp(s, "random") == 0) {
        *mode = MODE_RANDOM;
        return 0;
    }
    if (strcmp(s, "sequential") == 0) {
        *mode = MODE_SEQUENTIAL;
        return 0;
    }
    if (strcmp(s, "constant") == 0) {
        *mode = MODE_CONSTANT;
        return 0;
    }
    if (strcmp(s, "gaussian") == 0) {
        *mode = MODE_GAUSSIAN;
        return 0;
    }
    return -1;
}

static uint32_t next_random_u32(uint64_t *state) {
    /* xorshift64* PRNG, deterministic and fast for data generation. */
    uint64_t x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return (uint32_t)((x * UINT64_C(2685821657736338717)) >> 32);
}

static double next_uniform_open01(uint64_t *state) {
    return ((double)next_random_u32(state) + 1.0) / 4294967297.0;
}

static double next_gaussian(uint64_t *state, int *has_spare, double *spare) {
    if (*has_spare) {
        *has_spare = 0;
        return *spare;
    }

    for (;;) {
        double u = 2.0 * next_uniform_open01(state) - 1.0;
        double v = 2.0 * next_uniform_open01(state) - 1.0;
        double s = u * u + v * v;
        if (s <= 0.0 || s >= 1.0) {
            continue;
        }

        double m = sqrt(-2.0 * log(s) / s);
        *spare = v * m;
        *has_spare = 1;
        return u * m;
    }
}

static uint32_t gaussian_to_u32(double mean, double stddev, uint64_t *state, int *has_spare, double *spare) {
    double sample = mean + stddev * next_gaussian(state, has_spare, spare);
    if (sample <= 0.0) {
        return 0u;
    }
    if (sample >= (double)UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)(sample + 0.5);
}

int main(int argc, char **argv) {
    const char *out_path = NULL;
    uint64_t count = UINT64_MAX;
    Mode mode = MODE_RANDOM;
    uint64_t seed = 1;
    uint32_t start = 0;
    uint32_t constant_value = 0;
    double gaussian_mean = 2147483648.0;
    double gaussian_stddev = 1000000.0;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const char *val = NULL;

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }

        if (strcmp(arg, "-o") == 0 || strcmp(arg, "--output") == 0 ||
            strcmp(arg, "-n") == 0 || strcmp(arg, "--count") == 0 ||
            strcmp(arg, "-m") == 0 || strcmp(arg, "--mode") == 0 ||
            strcmp(arg, "-s") == 0 || strcmp(arg, "--seed") == 0 ||
            strcmp(arg, "--start") == 0 || strcmp(arg, "--value") == 0 ||
            strcmp(arg, "--mean") == 0 || strcmp(arg, "--stddev") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing value for %s\n", arg);
                return 1;
            }
            val = argv[++i];
        }

        if (strcmp(arg, "-o") == 0 || strcmp(arg, "--output") == 0) {
            out_path = val;
        } else if (strcmp(arg, "-n") == 0 || strcmp(arg, "--count") == 0) {
            if (parse_u64(val, &count) != 0) {
                fprintf(stderr, "Invalid count: %s\n", val);
                return 1;
            }
        } else if (strcmp(arg, "-m") == 0 || strcmp(arg, "--mode") == 0) {
            if (parse_mode(val, &mode) != 0) {
                fprintf(stderr, "Invalid mode: %s\n", val);
                return 1;
            }
        } else if (strcmp(arg, "-s") == 0 || strcmp(arg, "--seed") == 0) {
            if (parse_u64(val, &seed) != 0) {
                fprintf(stderr, "Invalid seed: %s\n", val);
                return 1;
            }
        } else if (strcmp(arg, "--start") == 0) {
            if (parse_u32(val, &start) != 0) {
                fprintf(stderr, "Invalid --start value: %s\n", val);
                return 1;
            }
        } else if (strcmp(arg, "--value") == 0) {
            if (parse_u32(val, &constant_value) != 0) {
                fprintf(stderr, "Invalid --value: %s\n", val);
                return 1;
            }
        } else if (strcmp(arg, "--mean") == 0) {
            if (parse_f64(val, &gaussian_mean) != 0) {
                fprintf(stderr, "Invalid --mean: %s\n", val);
                return 1;
            }
        } else if (strcmp(arg, "--stddev") == 0) {
            if (parse_f64(val, &gaussian_stddev) != 0) {
                fprintf(stderr, "Invalid --stddev: %s\n", val);
                return 1;
            }
        } else {
            fprintf(stderr, "Unknown argument: %s\n", arg);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (out_path == NULL || count == UINT64_MAX) {
        fprintf(stderr, "Both --output and --count are required.\n");
        print_usage(argv[0]);
        return 1;
    }

    if (count > MAX_COUNT) {
        fprintf(stderr, "Count is too large: %" PRIu64 " (max: %" PRIu64 ")\n", count, MAX_COUNT);
        return 1;
    }
    if (mode == MODE_GAUSSIAN && gaussian_stddev <= 0.0) {
        fprintf(stderr, "--stddev must be > 0 for gaussian mode\n");
        return 1;
    }

    FILE *f = fopen(out_path, "wb");
    if (f == NULL) {
        fprintf(stderr, "Failed to open %s: %s\n", out_path, strerror(errno));
        return 1;
    }

    uint32_t *buffer = (uint32_t *)malloc((size_t)CHUNK_INTS * sizeof(*buffer));
    if (buffer == NULL) {
        fprintf(stderr, "Failed to allocate buffer: %s\n", strerror(errno));
        fclose(f);
        return 1;
    }

    uint64_t generated = 0;
    uint64_t rng_state = seed == 0 ? UINT64_C(1) : seed;
    int gaussian_has_spare = 0;
    double gaussian_spare = 0.0;

    while (generated < count) {
        uint64_t remaining = count - generated;
        size_t chunk = (remaining > CHUNK_INTS) ? CHUNK_INTS : (size_t)remaining;

        if (mode == MODE_RANDOM) {
            for (size_t i = 0; i < chunk; ++i) {
                buffer[i] = next_random_u32(&rng_state);
            }
        } else if (mode == MODE_SEQUENTIAL) {
            for (size_t i = 0; i < chunk; ++i) {
                buffer[i] = (uint32_t)(start + (uint32_t)(generated + i));
            }
        } else if (mode == MODE_CONSTANT) {
            for (size_t i = 0; i < chunk; ++i) {
                buffer[i] = constant_value;
            }
        } else {
            for (size_t i = 0; i < chunk; ++i) {
                buffer[i] = gaussian_to_u32(
                    gaussian_mean, gaussian_stddev, &rng_state, &gaussian_has_spare, &gaussian_spare);
            }
        }

        size_t wrote = fwrite(buffer, sizeof(*buffer), chunk, f);
        if (wrote != chunk) {
            fprintf(stderr, "Write failed after %" PRIu64 " values: %s\n", generated, strerror(errno));
            free(buffer);
            fclose(f);
            return 1;
        }

        generated += chunk;
    }

    if (fclose(f) != 0) {
        fprintf(stderr, "Failed to close %s: %s\n", out_path, strerror(errno));
        free(buffer);
        return 1;
    }

    free(buffer);
    printf("Wrote %" PRIu64 " uint32_t values (%" PRIu64 " bytes) to %s\n",
           count, count * UINT64_C(4), out_path);
    return 0;
}
