#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
void fun(int* left) {
    close(left[1]);
    int p = 0;
    if(read(left[0], &p, 4) != 4) {
        return;
    }
    printf("prime %d\n", p);

    int pip1[2];
    if(pipe(pip1) == -1) {
        exit(1);
    }
    int n;
    while(read(left[0], &n, 4) == 4) {
        if(n % p != 0) {
            if(write(pip1[1], &n, 4) == -1)
                exit(1);
        }
    }
    
    int pid1 = fork();
    if(pid1 != 0) {
        close(pip1[1]);
        close(pip1[0]);
        close(left[0]);
        wait(0);
    }
    else {
        fun(pip1);
    }
}

int main(int argc, char *argv[]) {
    int pip[2];
    if(pipe(pip) == -1) {
        exit(1);
    }

    for(int i = 2; i < 36; i++) {
        if(write(pip[1], &i, 4) == -1)
            exit(1);
    }

    int pid = fork();
    if(pid == 0) {
        // child
        fun(pip);
    }
    else {
        close(pip[1]);
        close(pip[0]);
        wait(0);
    }
    exit(0);
}