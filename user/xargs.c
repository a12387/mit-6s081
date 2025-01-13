#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    char buf[512], *p;
    p = buf;
    while(read(0, p, 1) != 0) {
        if(*p == '\n' ) {
            *p = 0;
            int pid = fork();
            if(pid == 0) {
                char *arg[argc];
                for(int i = 1; i < argc; i++) {
                    arg[i - 1] = argv[i];
                }
                arg[argc - 1] = buf;
                if(exec(argv[1], arg) == -1) {
                    printf("%s", argv[1]);
                    for(int i =0; i < argc; i++) {
                        printf(" %s", arg[i]);
                    }
                    exit(0);
                }
            }
            else {
                p = buf;
                wait(0);
                continue;
            }
        }
        p++;
    }
    exit(0);
}