#include <sched.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void my_wait(int C)
{
  printf("wait %d\n", C);
}

int main()
{
  // Schedule
  struct sched_param scheduling_parameters;
  scheduling_parameters.sched_priority = 3;
  if(sched_setscheduler(getpid(), SCHED_FIFO, &scheduling_parameters) == -1)
  {
    perror("sched_setscheduler");
    exit(1);
  }

  // Calibrage
  struct timespec ref;
  clock_gettime(CLOCK_REALTIME, &ref);
  double t = (double)ref.tv_sec * 1e9 + (double)ref.tv_nsec;
  int k = 0;
  while(((double)ref.tv_sec * 1e9 + (double)ref.tv_nsec) < t + 1e6)
  {
    clock_gettime(CLOCK_REALTIME, &ref);
    k++;
  }
  printf("Calibrage k = %d\n", k);

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

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);

  sigset_t block;
  sigfillset(&block);
  sigdelset(&block, SIGRTMIN);
  sigdelset(&block, SIGALRM);
  sigprocmask(SIG_BLOCK, &block, NULL);

  struct sigaction sa;
  sa.sa_sigaction = my_wait;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;

  if(sigaction(SIGRTMIN, &sa, NULL) == -1)
  {
    perror("sigaction");
    exit(1);
  }

  alarm(4);

  pid_t child_pid;
  switch (child_pid = fork())
  {
    case -1:
    {
      perror("fork");
      exit(1);
    }
    case 0: // child
    {
      struct itimerspec new_setting, old_setting;
      new_setting.it_value.tv_sec = ts.tv_sec + 1;
      new_setting.it_value.tv_nsec = ts.tv_nsec;
      new_setting.it_interval.tv_sec = 6;
      new_setting.it_interval.tv_nsec = 0;

      if(timer_settime(timer, TIMER_ABSTIME, &new_setting, &old_setting) == -1)
      {
        perror("child_timer_settime");
        exit(1);
      }
    }
    default: // parent
    {
      struct sched_param child_scheduling_parameters;
      child_scheduling_parameters.sched_priority = 2;
      if(sched_setscheduler(child_pid, SCHED_FIFO, &child_scheduling_parameters) == -1)
      {
        perror("parent_sched_setscheduler");
        exit(1);
      }

      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      struct itimerspec new_setting, old_setting;
      new_setting.it_value.tv_sec = ts.tv_sec + 1;
      new_setting.it_value.tv_nsec = ts.tv_nsec;
      new_setting.it_interval.tv_sec = 4;
      new_setting.it_interval.tv_nsec = 0;

      if(timer_settime(timer, TIMER_ABSTIME, &new_setting, &old_setting) == -1)
      {
        perror("parent_timer_settime");
        exit(1);
      }
    }
  }

  return 0;
}
