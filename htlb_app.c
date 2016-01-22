#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFSIZE 12
#define FILE_PATH_NR_HP "/proc/sys/vm/nr_hugepages"

int parse_set(int argc, char *argv[])
{
    int args = -1;

    if (argc < 3) {
        return args;
    }

    if (strcmp(argv[1], "-s") != 0) {
        return args;
    }

    args = atoi(argv[2]);

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
    int ret = -1;

    if (set_val == -1) {
        return ret;
    }

    snprintf(buf, BUFSIZE*5, "echo %d > %s", set_val, FILE_PATH_NR_HP);
    ret = system(buf);
    printf("set: %d / %d\n", ret, set_val);

    return ret;
}

int main(int argc, char *argv[])
{
    int set_val;

    set_val = parse_set(argc, argv);

    print_cur_hp();

    if (set_val != -1) {
        set_hp(set_val);
        print_cur_hp();
    }

    return 0;
}
