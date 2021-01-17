#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>

/* #define CLOCKID CLOCK_REALTIME */
/* #define SIG SIGRTMIN */
/* #define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \ */
/*                         } while (0) */

/* static void */
/* print_siginfo(siginfo_t *si) */
/* { */
/*     timer_t *tidp; */
/*     int or; */
/*     tidp = si->si_value.sival_ptr; */
/*     printf("    sival_ptr = %p; ", si->si_value.sival_ptr); */
/*     printf("    *sival_ptr = %#jx\n", (uintmax_t) *tidp); */
/*     or = timer_getoverrun(*tidp); */
/*     if (or == -1) */
/*         errExit("timer_getoverrun"); */
/*     else */
/*         printf("    overrun count = %d\n", or); */
/* } */
/* static void */
/* handler(int sig, siginfo_t *si, void *uc) */
/* { */
/*     /1* Note: calling printf() from a signal handler is not safe */
/*        (and should not be done in production programs), since */
/*        printf() is not async-signal-safe; see signal-safety(7). */
/*        Nevertheless, we use printf() here as a simple way of */
/*        showing that the handler was called. *1/ */
/*     printf("Caught signal %d\n", sig); */
/*     print_siginfo(si); */
/*     signal(sig, SIG_IGN); */
/* } */

void my_fn(union sigval sv) {
    printf("in signal handler thread fn 'myfn'\n");
}

int
main(int argc, char *argv[])
{

    /* union sigval {            /1* Data passed with notification *1/ */
    /*     int     sival_int;    /1* Integer value *1/ */
    /*     void   *sival_ptr;    /1* Pointer value *1/ */
    /* }; */
    /**/
    /* struct sigevent { */
    /* int    sigev_notify;  /1* Notification method *1/ */
    /* int    sigev_signo;   /1* Notification signal *1/ */
    /* union sigval sigev_value; */
    /*                       /1* Data passed with notification *1/ */
    /* void (*sigev_notify_function) (union sigval); */
    /*                       /1* Function used for thread */
    /*                          notification (SIGEV_THREAD) *1/ */
    /* void  *sigev_notify_attributes; */
    /*                       /1* Attributes for notification thread */
    /*                          (SIGEV_THREAD) *1/ */
    /* pid_t  sigev_notify_thread_id; */
    /*                       /1* ID of thread to signal */
    /*                          (SIGEV_THREAD_ID); Linux-specific *1/ */
    /* }; */

    clockid_t clockid = CLOCK_REALTIME;

    union sigval sv = {
        .sival_int = 5
    };

    struct sigevent sev = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value = sv,
        .sigev_notify_function = &my_fn,
    };

    struct sigevent * sevp = &sev;

    timer_t timerid;
    timer_t * timeridp = &timerid;

    int timer = int timer_create(
            clockid,
            sevp,
            timeridp
            /* clockid_t clockid, */ 
            /* struct sigevent *sevp, */ 
            /* timer_t *timerid */
            );

    if (timer != 0) {
        /* printf("timer_create failed, errno: %i\n", errno); */
        perror("timer_create failed, errno: %i\n");
        exit(EXIT_FAILURE);
    }

    /* timer_t timerid; */
    /* struct sigevent sev; */
    /* struct itimerspec its; */
    /* long long freq_nanosecs; */
    /* sigset_t mask; */
    /* struct sigaction sa; */
    /* if (argc != 3) { */
    /*     fprintf(stderr, "Usage: %s <sleep-secs> <freq-nanosecs>\n", */
    /*             argv[0]); */
    /*     exit(EXIT_FAILURE); */
    /* } */
    /* /1* Establish handler for timer signal *1/ */
    /* printf("Establishing handler for signal %d\n", SIG); */
    /* sa.sa_flags = SA_SIGINFO; */
    /* sa.sa_sigaction = handler; */
    /* sigemptyset(&sa.sa_mask); */
    /* if (sigaction(SIG, &sa, NULL) == -1) */
    /*     errExit("sigaction"); */
    /* /1* Block timer signal temporarily *1/ */
    /* printf("Blocking signal %d\n", SIG); */
    /* sigemptyset(&mask); */
    /* sigaddset(&mask, SIG); */
    /* if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1) */
    /*     errExit("sigprocmask"); */
    /* /1* Create the timer *1/ */
    /* sev.sigev_notify = SIGEV_SIGNAL; */
    /* sev.sigev_signo = SIG; */
    /* sev.sigev_value.sival_ptr = &timerid; */
    /* if (timer_create(CLOCKID, &sev, &timerid) == -1) */
    /*     errExit("timer_create"); */
    /* printf("timer ID is %#jx\n", (uintmax_t) timerid); */
    /* /1* Start the timer *1/ */
    /* freq_nanosecs = atoll(argv[2]); */
    /* its.it_value.tv_sec = freq_nanosecs / 1000000000; */
    /* its.it_value.tv_nsec = freq_nanosecs % 1000000000; */
    /* its.it_interval.tv_sec = its.it_value.tv_sec; */
    /* its.it_interval.tv_nsec = its.it_value.tv_nsec; */
    /* if (timer_settime(timerid, 0, &its, NULL) == -1) */
    /*      errExit("timer_settime"); */
    /* /1* Sleep for a while; meanwhile, the timer may expire */
    /*    multiple times *1/ */
    /* printf("Sleeping for %d seconds\n", atoi(argv[1])); */
    /* sleep(atoi(argv[1])); */
    /* /1* Unlock the timer signal, so that timer notification */
    /*    can be delivered *1/ */
    /* printf("Unblocking signal %d\n", SIG); */
    /* if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) */
    /*     errExit("sigprocmask"); */

    exit(EXIT_SUCCESS);
}
