#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define BUFSIZE 12
#define LINELEN 255
#define FILE_PATH_NR_HP "/proc/sys/vm/nr_hugepages"
#define FILE_PATH_VMSTAT "/proc/vmstat"
#define FILE_PATH_AGGRESSIVE_ALLOC "/proc/sys/vm/hugepages_aggressive_alloc"
#define HTLB "htlb"
#define HTLBLEN (sizeof(HTLB) - 1)
#define FILE_PATH_AGGR_OUT "./results_test_app/aggr/"
#define FILE_PATH_NONAGGR_OUT "./results_test_app/nonaggr/"

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
mem_grab(int grab)
{
    void **ptrs, **ptrs_start = NULL;
    int ptr_alloc = (grab / (2 << 10)) * sizeof(void*);
    int tmp_grab = 10, ind = 0;

    if (grab <= 0) {
        return NULL;
    }
    /* Lazily get enough ptr storage for a ptr to every 2k bytes */
    ptrs_start = ptrs = malloc(ptr_alloc);
    grab -= ptr_alloc;
    /* Loop from 10 -> 11 -> 12 -> 13 -> 14 -> 10 */
    while (grab > 0) {
        tmp_grab = (tmp_grab == 14) ? 10 : (tmp_grab + 1);
        grab -= 2 << tmp_grab;
        *ptrs = malloc(2 << tmp_grab);
        for (ind = 0; ind < (2 << tmp_grab); ind++) {
            (*((char**) ptrs))[ind] = ind;
        }
        ptrs++;
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
redirect_output(char *args, int aggr_arg)
{
    char buf[LINELEN];
    int filefd;
    long curt = time(NULL);

    snprintf(buf, LINELEN, "%s/ta.%s.time%ld.out", ((aggr_arg > 0) ?
                    FILE_PATH_AGGR_OUT : FILE_PATH_NONAGGR_OUT),
            args, curt);
    filefd = creat(buf, 00444);
    if (filefd == -1) {
        printf("Err: Can't open output file '%s'.\n", buf);
        return -1;
    }
    if (dup2(filefd, STDOUT_FILENO) == -1) {
        printf("Err: Can't redirect output.\n");
        return -1;
    }
    return 1;
}

int
main(int argc, char *argv[])
{
    int set_arg = -1, grab_arg = 0, iter_arg = 1, tmp_arg = -1, file_arg = 0, aggr_arg = 1, mmap_arg = -1, reset_arg = 1;
    int argind = 1, ret = 0, iter = 0;
    int accumulator[6] = { 0 };

    while (argind < argc) {
        switch (parse_arg(argc, argv, argind, &tmp_arg)) {
        case 'r':
            reset_arg = tmp_arg;
            break;
        case 's':
            set_arg = tmp_arg;
            break;
        case 'a':
            aggr_arg = tmp_arg;
            break;
        case 'g':
            if (tmp_arg > 0) {
                grab_arg = (2 << tmp_arg);
            }
            break;
        case 'm':
           if (tmp_arg > 0) {
               mmap_arg = (2 << tmp_arg);
            }
            break;
        case 'i':
            iter_arg = tmp_arg;
            break;
        case 'f':
            file_arg = 1;
            break;
        default:
            printf("Read the source for the usage.\n");
            ret = 1;
            goto out;
        }
        argind += 2;
    }
    if (file_arg > 0) {
        char buf[BUFSIZE * 5];
        snprintf(buf, BUFSIZE * 5, "a%d.s%d.g%d.i%d.r%d",
                aggr_arg, set_arg, grab_arg, iter_arg, reset_arg);
        if (redirect_output(buf, aggr_arg) == -1) {
            ret = errno;
            goto out;
        }
        printf("args: %s\n", buf);
    }
    if (set_aggr(aggr_arg) == -1) {
        fprintf(stderr, "hugepages_aggressiv_alloc set fail\n");
        ret = 1;
        goto out;
    }
    /* could mlockall */
    do {
        int old[6] = { 0 }, new[6] = { 0 };
        int set_tmp = set_arg;
        int grab_tmp = grab_arg;
        int mmap_tmp = mmap_arg;
        void **ptr, **start_ptr = NULL, *mmap_ptr = MAP_FAILED;

        /* reset by default */
        if (reset_arg != 0) {
            set_hp(0, 0);
        }
        iter++;
        printf("iter args: s %d g %d i %d\n", set_tmp, grab_tmp, iter);
        if (grab_arg > 0) {
            /* Could release but exit also releases */
            start_ptr = ptr = mem_grab(grab_tmp);
            if (ptr == NULL) {
                fprintf(stderr, "failed to grab mem\n");
                ret = 1;
                goto out;
            } else {
                printf("grabbed %d\n", 2 << grab_tmp);
            }
        }
        if (mmap_arg > -1) {
            if ((mmap_ptr = mmap_grab(mmap_tmp)) == MAP_FAILED) {
                fprintf(stderr, "failed to grab mem\n");
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
        while ((start_ptr != NULL) && (*ptr != NULL)) {
            free(*ptr);
            ptr++;
        }
        free(start_ptr);
        if (mmap_ptr != MAP_FAILED) {
            munmap(mmap_ptr, mmap_tmp);
        }
    } while (iter < iter_arg);

    if (set_arg > -1) {
        print_end_stats(accumulator, iter_arg);
    }
 out:
    set_hp(0, 0);
    return ret;
}
