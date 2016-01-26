#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

#define BUFSIZE 12
#define FILE_PATH_NR_HP "/proc/sys/vm/nr_hugepages"

int parse_arg(int argc, char *argv[], int argind, char *str)
{
    int args = -1;

    if (argc < argind + 1) {
        return args;
    }

    if (strcmp(argv[argind], str) != 0) {
        return args;
    }

    args = atoi(argv[argind + 1]);

    return args;
}

int print_cur_hp(void)
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
        printf("curhp: %s", cur_hp);
    }
    fclose(f_nr_hp);
    return ret;
}

int set_hp(int set_val)
{
    char buf[BUFSIZE * 5];
    int ret;

    switch (fork()) {
    case 0:
        if (set_val == -1) {
            return -1;
        }
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
}

int grab_mem(int grab)
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

int main(int argc, char *argv[])
{
    int set_val, argind = 1, grab_val;

    set_val = parse_arg(argc, argv, argind, "-s");
    if (set_val != -1) {
        argind += 2;
    }
    grab_val = parse_arg(argc, argv, argind, "-g");

    print_cur_hp();

    if (grab_val != -1) {
        mlockall(MCL_FUTURE);
        /* Could release but exit also releases */
        if (grab_mem(grab_val) == -1) {
            printf("failed to mmap\n");
        } else {
            printf("mmap'd %d\n", 2 << grab_val);
            sleep(10);
        }
    }

    if (set_val != -1) {
        set_hp(set_val);
        print_cur_hp();
    }

    return 0;
}
