#include <sched.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef _POSIX_PRIORITY_SCHEDULING
#error "No Posix Priority Scheduling"
#endif

struct timespec start;

void handler()
{
  struct timespec current;
  clock_gettime(CLOCK_REALTIME, &current);
  double interval = (double)current.tv_sec * 1e9 + (double) current.tv_nsec -
             (double)start.tv_sec * 1e9 + (double) start.tv_nsec;
  printf("Temps écoulé = % 1fs // %.0fns\n", interval/1e9, interval);
}

int main()
{
  // Schedule
  struct sched_param scheduling_parameters;
  scheduling_parameters.sched_priority = 17;
  if(sched_setscheduler(getpid(), SCHED_FIFO, &scheduling_parameters) == -1)
  {
    perror("sched_setscheduler");
    exit(1);
  }


  struct sigevent sevp;
  sevp.sigev_notify = SIGEV_SIGNAL;
  sevp.sigev_signo = SIGRTMIN;

  // Timer
  timer_t timer;
  if(timer_create(CLOCK_REALTIME, &sevp, &timer) == -1)
  {
    perror("timer_create");
    exit(1);
  }

  struct itimerspec new_setting, old_setting;
  new_setting.it_value.tv_sec = 1;
  new_setting.it_value.tv_nsec = 0;
  new_setting.it_interval.tv_sec = 1;
  new_setting.it_interval.tv_nsec = 0;

  sigset_t block;
  sigfillset(&block);
  sigdelset(&block, SIGRTMIN);
  sigdelset(&block, SIGALRM);
  sigprocmask(SIG_BLOCK, &block, NULL);

  struct sigaction sa;
  sa.sa_sigaction = handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;

  if(sigaction(SIGRTMIN, &sa, NULL) == -1)
  {
    perror("sigaction");
    exit(1);
  }

  struct timespec realtime_res;
  clock_getres(CLOCK_REALTIME, &realtime_res);
  printf("Clock resolution: %ds et %lins\n", (int)realtime_res.tv_sec, realtime_res.tv_nsec);

  alarm(30);

  clock_gettime(CLOCK_REALTIME, &start);
  if(timer_settime(timer, 0, &new_setting, &old_setting) == -1)
  {
    perror("timer_settime");
    exit(1);
  }

  while(1)
  {
    sigsuspend(&block);
  }

  return 0;
}
