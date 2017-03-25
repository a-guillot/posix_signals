#include <sched.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "check.h"

#ifndef _POSIX_PRIORITY_SCHEDULING
#error "No Posix Priority Scheduling"
#endif

/* Global Variables */
struct timespec start; // used to see how much time has passed since the
                       // start of the program

/* Called when SIGRTMIN is received */
void handler()
{
  struct timespec current;
  CHECK(clock_gettime(CLOCK_REALTIME, &current) != -1);

  double interval = (double)current.tv_sec * 1e9 + (double) current.tv_nsec -
             (double)start.tv_sec * 1e9 + (double) start.tv_nsec;
  printf("Temps écoulé = %.0fs, %.0f ns\n", interval/1e9, interval);
}

int main()
{
  /* Variables */
  struct sched_param scheduling_parameters; // set process' parameters
  timer_t timer; // timer used to measure this program's precision
  struct sigevent sevp; // structure specifying the timer's behaviour
  sigset_t block; // set containing every signal that we wish to block
  struct sigaction sa; // struture linking SIGRTMIN to handler()
  struct timespec realtime_res; // used to print clock's resolution
  struct itimerspec new_setting; // will define when and how often the timer
                                 // will expire

  /* Set scheduling options to FIFO, with priority=17 */
  scheduling_parameters.sched_priority = 17;
  CHECK(sched_setscheduler(getpid(), SCHED_FIFO, &scheduling_parameters) != -1);

  /* Blocking everything except SIGRTMIN, SIGALARM and SIGINT*/
  sigfillset(&block);
  sigdelset(&block, SIGRTMIN); // SIGRTMIN for our measurements
  sigdelset(&block, SIGALRM);  // SIGALRM to stop the program
  sigdelset(&block, SIGINT);   // SIGINT to be able to interrupt

  CHECK(sigprocmask(SIG_BLOCK, &block, NULL) != -1);

  /* Receiving SIGRTMIN => handler() */
  sa.sa_sigaction = handler; // call handler()
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  CHECK(sigaction(SIGRTMIN, &sa, NULL) != -1);

  /* Creating a timer that generates a SIGRTMIN signal when it expires */
  sevp.sigev_notify = SIGEV_SIGNAL;
  sevp.sigev_signo = SIGRTMIN;

  CHECK(timer_create(CLOCK_REALTIME, &sevp, &timer) != -1);

  /* Printf clock's resolution */
  CHECK(clock_getres(CLOCK_REALTIME, &realtime_res) != -1);
  printf("Clock resolution: %ds, %lins\n",
        (int)realtime_res.tv_sec,
        realtime_res.tv_nsec);

  /* The timer expires for the first time at one second, and once every second
    After That */
  new_setting.it_value.tv_sec = 1;
  new_setting.it_value.tv_nsec = 0;
  new_setting.it_interval.tv_sec = 1;
  new_setting.it_interval.tv_nsec = 0;

  CHECK(clock_gettime(CLOCK_REALTIME, &start) != -1); // reference
  CHECK(timer_settime(timer, 0, &new_setting, NULL) != -1); // start timer

  /* Set the program to stop after 30 seconds */
  alarm(30);

  /* Indefinitely wait for signals */
  while(1)
    sigsuspend(&block); // wait for signals

  exit(EXIT_SUCCESS);
}
