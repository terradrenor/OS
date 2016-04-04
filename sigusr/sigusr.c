#include <stdio.h>
#include <signal.h>

void handler(int sig, siginfo_t *sig_info, void *smth) {
    if (sig == SIGUSR1) {
        printf("SIGUSR1 from %d\n", sig_info->si_pid);
    } else {
        printf("SIGUSR2 from %d\n", sig_info->si_pid);
    }
}

int main(int argc, char *argv[]) {
    struct sigaction act = (struct sigaction) {
        .sa_sigaction = handler,
        .sa_flags = SA_SIGINFO
    };

    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGUSR2, &act, NULL);

    if (sleep(10) == 0) {
        printf("No signals were caught\n");
    }
    
    return 0;
}
