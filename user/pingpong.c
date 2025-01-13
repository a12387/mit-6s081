#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    char buf[2];
    int pip[2];
    int pip_ret = pipe(pip);
    if(pip_ret == -1) {
        exit(-1);
    }
    int pid = fork();
    if(pid == 0) {
        read(pip[0], buf, 1);
        printf("%d: received ping\n", getpid());
        write(pip[1], "1", 1);
    }
    else {
        write(pip[1], "0", 1);
        wait(0);
        read(pip[0], buf, 1);
        printf("%d: received pong\n", getpid());
    }

    exit(0);
}