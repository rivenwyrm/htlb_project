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
get_stats(int old[6], int counters[6])
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

    printf("%s alloc_success: %d\n", HTLB, counters[0] - old[0]);
    printf("%s alloc_fail: %d\n", HTLB,counters[1] - old[1]);
    printf("%s retry_success: %d\n", HTLB, counters[2] - old[2]);
    printf("%s retry_fail: %d\n", HTLB, counters[3] - old[3]);
    printf("%s subseq_fail: %d\n", HTLB, counters[4] - old[4]);
    printf("%s subseq_success: %d\n", HTLB, counters[5] - old[5]);
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
    return ret;
}

int
grab_mem(int grab)
{
    if (grab == -1) {
        return -1;
    }

    if (mmap(0, 2 << grab, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0) == MAP_FAILED) {
        return -1;
    } else {
        return 0;
    }
}

int
main(int argc, char *argv[])
{
    int set_val = -1, grab_val = -1, iter_val = -1, tmp_val = -1, fuzz_val = 0;
    int argind = 1, ret = 0;

    while (argind < argc) {
        switch (parse_arg(argc, argv, argind, &tmp_val)) {
        case 's':
            set_val = tmp_val;
            break;
        case 'g':
            grab_val = tmp_val;
            break;
        case 'i':
            iter_val = tmp_val;
            break;
        case 'f':
            fuzz_val = tmp_val;
            break;
        default:
            printf("Read the source for the usage.\n");
            ret = 1;
            goto out;
        }
        argind += 2;
    }
    printf("args: s %d g %d i %d f %d\n", set_val, grab_val, iter_val, fuzz_val);
    do {
        int old[6] = { 0 }, new[6] = { 0 };
        int set = set_val + (((rand() / RAND_MAX) * fuzz_val));
        int grab = grab_val + (((rand() / RAND_MAX) * fuzz_val));
        set_hp(0, 0);

        iter_val--;
        print_cur_hp();
        printf("iter args: s %d g %d i %d\n", set, grab, iter_val);
        if (grab_val > -1) {
            mlockall(MCL_FUTURE);
            /* Could release but exit also releases */
            if (grab_mem(grab) == -1) {
                printf("failed to mmap\n");
                ret = 1;
                goto out;
            } else {
                printf("mmap'd %d\n", 2 << grab);
            }
        }
        printf("initial stats\n");
        get_stats(old, new);
        if (set_val > -1) {
            set_hp(set, 0);
            print_cur_hp();
            printf("relative change stats\n");
            get_stats(new, old);
        }
    } while (iter_val > 0);

 out:
    return ret;
}
