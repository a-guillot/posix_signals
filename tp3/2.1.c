#include <sched.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int C = 0;
int k = 0;
struct timespec ref, ref2;

timer_t timer;
timer_t timer_child;

void my_wait(int signo)
{
  int i;
  clock_gettime(CLOCK_REALTIME, &ref2);
  printf("Démarrage T%d a %d s et %li ns", ((C == 4) +1), (int)ref2.tv_sec, ref2.tv_nsec);

  for (i = 1; i < (C * k); i++)
  {
    clock_gettime(CLOCK_REALTIME, &ref);
  }
  printf("\nFin de T%d a %d s et %li ns", ((C == 4) +1), (int)ref.tv_sec, ref.tv_nsec);
  printf("\nDurée : %lins\n\n", ref.tv_nsec - ref2.tv_nsec);

  int overtime;
  if (timer == NULL)
    overtime = timer_getoverrun(timer_child);
  else
    overtime = timer_getoverrun(timer);

  if (overtime == -1)
    printf("error\n");
  else
    printf("(c : overtime) = (%d : %d)\n", C, overtime);
}

int main()
{
  /* Declare the scheduling parameters as :
    - */
  struct sched_param scheduling_parameters;
  scheduling_parameters.sched_priority = 3;
  if(sched_setscheduler(getpid(), SCHED_FIFO, &scheduling_parameters) == -1)
  {
    perror("sched_setscheduler");
    exit(1);
  }

  // Calibrage
  clock_gettime(CLOCK_REALTIME, &ref);
  double t = (double)ref.tv_sec * 1e9 + (double)ref.tv_nsec;
  while(((double)ref.tv_sec * 1e9 + (double)ref.tv_nsec) < t + 1e6)
  {
    clock_gettime(CLOCK_REALTIME, &ref);
    k++;
  }
  printf("Calibrage k = %d\n\n", k);

  struct sigevent sevp;
  sevp.sigev_notify = SIGEV_SIGNAL;
  sevp.sigev_signo = SIGRTMIN;

  // Timer
  if(timer_create(CLOCK_REALTIME, &sevp, &timer) == -1)
  {
    perror("timer_create");
    exit(1);
  }

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);

  sigset_t block;
  sigemptyset(&block);

  struct sigaction sa;
  sa.sa_flags = 0;
  sa.sa_handler = my_wait;
  sigemptyset(&sa.sa_mask);

  if(sigaction(SIGRTMIN, &sa, NULL) == -1)
  {
    perror("sigaction");
    exit(1);
  }

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
      alarm(50);

      if(timer_create(CLOCK_REALTIME, &sevp, &timer_child) == -1)
      {
        perror("timer_create");
        exit(1);
      }

      struct itimerspec new_setting;
      new_setting.it_value.tv_sec = ts.tv_sec + 1;
      new_setting.it_value.tv_nsec = ts.tv_nsec;
      new_setting.it_interval.tv_sec = 6;
      new_setting.it_interval.tv_nsec = 0;

      C = 5000;
      timer = NULL;

      if(timer_settime(timer_child, TIMER_ABSTIME, &new_setting, NULL) != 0)
      {
        fprintf(stderr, "Erreur dans le timer settime du fils\n");
        perror("timer_settime");
        exit(1);
      }

      break;
    }
    default: // parent
    {
      alarm(50);
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

      C = 5;

      if(timer_settime(timer, TIMER_ABSTIME, &new_setting, &old_setting) == -1)
      {
        perror("parent_timer_settime");
        exit(1);
      }
    }
  }

  while (1)
  {
    sigsuspend(&block);
  }

  return 0;
}
