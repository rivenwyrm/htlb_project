#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

#define BUFSIZE 12
#define LINELEN 255
#define FILE_PATH_NR_HP "/proc/sys/vm/nr_hugepages"
#define FILE_PATH_VMSTAT "/proc/vmstat"
#define FILE_PATH_AGGRESSIVE_ALLOC "/proc/sys/vm/hugepages_aggressive_alloc"
#define HTLB "htlb"
#define HTLBLEN (sizeof(HTLB) - 1)

int
parse_arg(int argc, char *argv[], int argind, int *val)
{
    int arg = -1;

    if (argc > argind) {
        /* -X */
        arg = argv[argind][1];

        if (argc > argind + 1) {
            *val = atoi(argv[argind + 1]);
        }
    }
    return arg;
}

int
print_cur_hp(void)
{
    char cur_hp[BUFSIZE] = {0};
    FILE *f_nr_hp = fopen(FILE_PATH_NR_HP, "r");
    int ret = 0;

    if (f_nr_hp == NULL) {
        printf("fopen err\n");
        ret = 1;
        return ret;
    }

    if (fgets(cur_hp, BUFSIZE, f_nr_hp) == NULL) {
        printf("fgets err\n");
        ret = 1;
    } else {
        printf("curhp: %s\n", cur_hp);
    }
    fclose(f_nr_hp);
    return ret;
}

/*
 * htlb_buddy_alloc_success 300
 * htlb_buddy_alloc_fail 7
 * htlb_buddy_pgalloc_retry_success 4
 * htlb_buddy_pgalloc_retry_fail 7
 * htlb_buddy_pgalloc_subseq_fail 2
 * htlb_buddy_pgalloc_subseq_success 8
*/
void
get_stats(int old[6], int counters[6], int accumulator[6], int print)
{
    char cur[LINELEN];
    FILE *f_vmstat = fopen(FILE_PATH_VMSTAT, "r");
    int ind = 0;

    if (f_vmstat == NULL) { /* Should never happen */
        printf("fopen err\n");
        return;
    }

    while (fgets(cur, LINELEN, f_vmstat) != NULL) {
        if (strncmp(cur, HTLB, HTLBLEN) == 0) {
            break;
        }
    }
    do {
        counters[ind++] = atoi(strchr(cur, ' ') + 1);
        fgets(cur, LINELEN, f_vmstat);
    } while (ind < 6);
    fclose(f_vmstat);

    if (!print) {
        return;
    }

    printf("%s alloc_success: %d\n", HTLB, counters[0] - old[0]);
    printf("%s alloc_fail: %d\n", HTLB,counters[1] - old[1]);
    printf("%s retry_success: %d\n", HTLB, counters[2] - old[2]);
    printf("%s retry_fail: %d\n", HTLB, counters[3] - old[3]);
    printf("%s subseq_fail: %d\n", HTLB, counters[4] - old[4]);
    printf("%s subseq_success: %d\n", HTLB, counters[5] - old[5]);

    if (accumulator) {
        accumulator[0] += counters[0] - old[0];
        accumulator[1] += counters[1] - old[1];
        accumulator[2] += counters[2] - old[2];
        accumulator[3] += counters[3] - old[3];
        accumulator[4] += counters[4] - old[4];
        accumulator[5] += counters[5] - old[5];
    }
}

int
set_hp(int set_val, int forkmode)
{
    char buf[BUFSIZE * 5];
    int ret;

    if (set_val == -1) {
        return -1;
    }

    if (forkmode) {
        switch (fork()) {
        case 0:
            snprintf(buf, BUFSIZE*5, "echo %d > %s", set_val, FILE_PATH_NR_HP);
            ret = system(buf);
            printf("set: %d / %d\n", ret, set_val);

            exit(0);
        case EAGAIN:
        case ENOMEM:
            return -1;
        default:
            return 0;
        }
    } else {
        snprintf(buf, BUFSIZE*5, "echo %d > %s", set_val, FILE_PATH_NR_HP);
        ret = system(buf);
        printf("set: %d / %d\n", ret, set_val);
    }
    print_cur_hp();
    return ret;
}

int
set_aggr(int set_val)
{
    char buf[BUFSIZE * 5];
    int ret;

    if (set_val == -1) {
        return -1;
    }

    snprintf(buf, BUFSIZE*5, "echo %d > %s", set_val, FILE_PATH_AGGRESSIVE_ALLOC);
    ret = system(buf);
    printf("aggr: %d / %d\n", ret, set_val);
    return ret;
}

void *
mmap_grab(int grab)
{
    if (grab == -1) {
        return NULL;
    }

    return mmap(0, grab, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, 0, 0);
}

void **
random_grab(int grab)
{
    void **ptrs, **ptrs_start = NULL;

    if (grab <= 0) {
        return NULL;
    }
    ptrs_start = ptrs = malloc((grab / 2) / sizeof(void*)); /* Statistically this should be enough */
    grab -= ((grab / 2) / sizeof(void*));
    /* Just segfault if we don't get ptrs */
    while (grab > (2 << 10)) {
        int tmp_grab = grab * (((float) rand()) / ((float) RAND_MAX));
        grab -= tmp_grab;
        *ptrs++ = malloc(tmp_grab);
    }
    *ptrs = NULL;

    return ptrs_start;
}

void
print_end_stats(int accumulator[6], int iter_val)
{
        printf("abs\n");
        printf("%s alloc_success: %d\n", "abs", accumulator[0]);
        printf("%s alloc_fail: %d\n", "abs", accumulator[1]);
        printf("%s retry_success: %d\n", "abs", accumulator[2]);
        printf("%s retry_fail: %d\n", "abs", accumulator[3]);
        printf("%s subseq_fail: %d\n", "abs", accumulator[4]);
        printf("%s subseq_success: %d\n", "abs", accumulator[5]);
        printf("avgs\n");
        printf("%s alloc_success: %d\n", "avg", accumulator[0] / iter_val);
        printf("%s alloc_fail: %d\n", "avg", accumulator[1] / iter_val);
        printf("%s retry_success: %d\n", "avg", accumulator[2] / iter_val);
        printf("%s retry_fail: %d\n", "avg", accumulator[3] / iter_val);
        printf("%s subseq_fail: %d\n", "avg", accumulator[4] / iter_val);
        printf("%s subseq_success: %d\n", "avg", accumulator[5] / iter_val);
}

int
main(int argc, char *argv[])
{
    int set_arg = -1, grab_arg = -1, iter_arg = 1, tmp_arg = -1, fuzz_arg = 0, aggr_arg = 1, mmap_arg = -1;
    int argind = 1, ret = 0, iter = 0;
    int accumulator[6] = { 0 };

    while (argind < argc) {
        switch (parse_arg(argc, argv, argind, &tmp_arg)) {
        case 's':
            set_arg = tmp_arg;
            break;
        case 'a':
            aggr_arg = tmp_arg;
            break;
        case 'g':
            grab_arg = tmp_arg;
            grab_arg = 2 << grab_arg;
            break;
        case 'm':
            mmap_arg = tmp_arg;
            mmap_arg = 2 << mmap_arg;
            break;
        case 'i':
            iter_arg = tmp_arg;
            break;
        case 'f':
            fuzz_arg = tmp_arg;
            break;
        default:
            printf("Read the source for the usage.\n");
            ret = 1;
            goto out;
        }
        argind += 2;
    }
    printf("args: a %d s %d g %d i %d f %d\n", aggr_arg, set_arg, grab_arg, iter_arg, fuzz_arg);
    mlockall(MCL_FUTURE); /* Doesn't matter for hugepages but does for other allocs */
    if (set_aggr(aggr_arg) == -1) {
        printf("hugepages_aggressiv_alloc set fail\n");
        ret = 1;
        goto out;
    }

    do {
        int old[6] = { 0 }, new[6] = { 0 };
        int set_tmp = set_arg + (((rand() / RAND_MAX) * fuzz_arg));
        int grab_tmp = grab_arg + (((rand() / RAND_MAX) * fuzz_arg));
        int mmap_tmp = mmap_arg + (((rand() / RAND_MAX) * fuzz_arg));
        void **ptr, *mmap_ptr = MAP_FAILED;

        /* Initialize and muck with mem */
        set_hp(0, 0);
        iter++;
        printf("iter args: s %d g %d i %d\n", set_tmp, grab_tmp, iter);
        if (grab_arg > -1) {
            /* Could release but exit also releases */
            ptr = random_grab(grab_tmp);
            if (ptr == NULL) {
                printf("failed to grab mem\n");
                ret = 1;
                goto out;
            } else {
                printf("grabbed %d\n", 2 << grab_tmp);
            }
        }
        if (mmap_arg > -1) {
            if ((mmap_ptr = mmap_grab(mmap_tmp)) == MAP_FAILED) {
                printf("failed to grab mem\n");
                ret = 1;
                goto out;
            } else {
                printf("grabbed %d\n", 2 << mmap_tmp);
            }
        }
        get_stats(old, new, NULL, 1);
        /* Do the actual hugepage work */
        if (set_arg > -1) {
            set_hp(set_tmp, 0);
            printf("relative change stats\n");
            get_stats(new, old, accumulator, 1);
        }
        /* Cleanup */
        while (*ptr != NULL) {
            free(*ptr);
            ptr++;
        }
        free(ptr);
        if (mmap_ptr != MAP_FAILED) {
            munmap(mmap_ptr, mmap_tmp);
        }
    } while (iter < iter_arg);

    if (set_arg > -1) {
        print_end_stats(accumulator, iter_arg);
    }
 out:
    return ret;
}
