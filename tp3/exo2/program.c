#include <sched.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "check.h"

/* Global Variables */
int C = 0; // capacity
unsigned int k = 0; // calibration

struct timespec ref, ref2;

// timers
timer_t timer_parent;
timer_t timer_child;

void handler()
{
  clock_gettime(CLOCK_REALTIME, &ref2);

  printf("Démarrage T%d a %d s et %li ns",
      ((C == 4) +1),
      (int)ref2.tv_sec,
      ref2.tv_nsec);

  for (size_t i = 1; i < (C * k); i++)
    clock_gettime(CLOCK_REALTIME, &ref);

  printf("\nFin de T%d a %d s et %li ns",
      ((C == 4) +1),
      (int)ref.tv_sec, ref.tv_nsec);

  printf("\nDurée : %lins\n\n", ref.tv_nsec - ref2.tv_nsec);

  int overtime;
  if (timer_parent == NULL)
    CHECK((overtime = timer_getoverrun(timer_child)) != -1);
  else
    CHECK((overtime = timer_getoverrun(timer_parent)) != -1);

  if (overtime > 0)
    printf("T%d overtimed!\n", ((C == 4) + 1));
}

int main()
{
  /* Variables */
  struct sched_param scheduling_parameters; // set process' parameters
  sigset_t block; // set containing every signal that we wish to block
  struct sigaction sa; // struture linking SIGRTMIN to handler()
  struct sigevent sevp; // structure specifying the timer's behaviour
  struct itimerspec time_parameters;

  /* Set scheduling options to FIFO, with priority=3 */
  scheduling_parameters.sched_priority = 3;
  CHECK(sched_setscheduler(getpid(), SCHED_FIFO, &scheduling_parameters) != -1);

  /* Blocking everything except SIGRTMIN, SIGALARM and SIGINT*/
  sigfillset(&block);
  sigdelset(&block, SIGRTMIN); // SIGRTMIN for our measurements
  sigdelset(&block, SIGALRM);  // SIGALRM to stop the program
  sigdelset(&block, SIGINT);   // SIGINT to be able to interrupt

  CHECK(sigaction(SIGRTMIN, &sa, NULL) != -1);

  /* Receiving SIGRTMIN => handler() */
  sa.sa_sigaction = handler; // call handler()
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  CHECK(sigaction(SIGRTMIN, &sa, NULL) != -1);

  /* Setting timer action */
  sevp.sigev_notify = SIGEV_SIGNAL;
  sevp.sigev_signo = SIGRTMIN;

  /* Calibrating */
  CHECK(clock_gettime(CLOCK_REALTIME, &ref) != -1);
  double t = (double)ref.tv_sec * 1e9 + (double)ref.tv_nsec;
  while(((double)ref.tv_sec * 1e9 + (double)ref.tv_nsec) < t + 1e6)
  {
    clock_gettime(CLOCK_REALTIME, &ref);
    k++;
  }
  printf("Calibrage k = %d\n\n", k);

  /* Creating 2 processes */
  pid_t child;
  CHECK((child = fork()) != -1);

  if (!child) // child
  {
    /* Set capacity to 4 */
    C = 4;

    /* Change child's priority to 2 */
    scheduling_parameters.sched_priority = 2;
    CHECK(sched_setscheduler(child, SCHED_FIFO, &scheduling_parameters) != -1);

    /* Creating the timer */
    CHECK(timer_create(CLOCK_REALTIME, &sevp, &timer_child) != -1);

    /* Setting the timer to an absolute time (ref + 1 second) with period = 6 */
    time_parameters.it_value.tv_sec = ref.tv_sec + 1;
    time_parameters.it_value.tv_nsec = ref.tv_nsec;
    time_parameters.it_interval.tv_sec = 0;
    time_parameters.it_interval.tv_nsec = 6 / 1e6;

    CHECK(timer_settime(timer_child, TIMER_ABSTIME, &time_parameters, NULL)
                != -1);
  }
  else // parent
  {
    /* Set capacity to 2 */
    C = 2;

    /* Change child's priority to 2 */
    scheduling_parameters.sched_priority = 2;
    CHECK(sched_setscheduler(child, SCHED_FIFO, &scheduling_parameters) != -1);

    /* Creating the timer */
    CHECK(timer_create(CLOCK_REALTIME, &sevp, &timer_parent) != -1);

    /* Setting the timer to an absolute time (ref + 1 second) with period = 4 */
    time_parameters.it_value.tv_sec = ref.tv_sec + 1;
    time_parameters.it_value.tv_nsec = ref.tv_nsec;
    time_parameters.it_interval.tv_sec = 0;
    time_parameters.it_interval.tv_nsec = 4 * 1e6;

    CHECK(timer_settime(timer_parent, TIMER_ABSTIME, &time_parameters, NULL)
                != -1);
  }
  alarm(30);

  /* Indefinitely wait for signals */
  while (1)
    sigsuspend(&block);

  exit(EXIT_SUCCESS);
}
