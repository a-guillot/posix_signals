#include <sched.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mqueue.h>
#include "check.h"

#define MAXMSG 20
#define MSGSIZE 1024
#define MQ_NAME "/queue"

/* Global Variables */
int C = 0; // capacity
unsigned int k = 0; // calibration
mqd_t queue; // used to print messages

// timers
timer_t timer_parent;
timer_t timer_child;

void handler()
{
  /* ADDED: sending strings to another process via the queue */
  struct timespec start, end;
  int id = (C == 4) + 1; // allows us to tell if it is T1 or T2
  char buffer[MSGSIZE]; // sending buffer
  buffer[0] = 0; // reset the string;

  clock_gettime(CLOCK_REALTIME, &start);

  CHECK((snprintf(buffer, MSGSIZE, "Démarrage T%d a %d s et %li ns.\n",
        id, (int)start.tv_sec, start.tv_nsec) != -1));

  CHECK((mq_send(queue, buffer, MSGSIZE, 0) != -1));

  for (size_t i = 1; i < (C * k); i++)
    clock_gettime(CLOCK_REALTIME, &end);

  CHECK((snprintf(buffer,MSGSIZE,"Fin de T%d a %d s et %li ns\nDurée : %lins\n",
     id, (int)end.tv_sec, end.tv_nsec, (end.tv_nsec - start.tv_nsec))) != -1);

  CHECK((mq_send(queue, buffer, MSGSIZE, 0) != -1));

  int overrun_number = 0;
  timer_t * my_timer; // allows us to use the correct one

  if (id == 1)
    my_timer = &timer_parent;
  else
    my_timer = &timer_child;

  CHECK((overrun_number = timer_getoverrun(*my_timer)) != -1);

  if (overrun_number)
  {
    CHECK((snprintf(buffer, MSGSIZE, "T%d -----> %d échéances ratées\n",
       id, overrun_number)) != -1);

    CHECK((mq_send(queue, buffer, MSGSIZE, 0) != -1));
  }
}

int main()
{
  /* Variables */
  struct sched_param scheduling_parameters; // set process' parameters
  sigset_t block; // set containing every signal that we wish to block
  struct sigaction sa; // struture linking SIGRTMIN to handler()
  struct sigevent sevp; // structure specifying the timer's behaviour
  struct itimerspec time_parameters;
  struct timespec ref; // used for calibration purposes
  struct mq_attr queue_attributes;

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

  /* ADDED: Queue management */
  queue_attributes.mq_maxmsg = MAXMSG;
  queue_attributes.mq_msgsize = MSGSIZE;
  queue_attributes.mq_flags = 0;

  CHECK((queue = mq_open(MQ_NAME, O_CREAT|O_RDWR, S_IRWXU, &queue_attributes))
          != (mqd_t)-1); // queue creation

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
    time_parameters.it_interval.tv_nsec = 6 * 1e6;

    CHECK(timer_settime(timer_child, TIMER_ABSTIME, &time_parameters, NULL)
                != -1);
  }
  else // parent
  {
    /* Change child's priority to 2 */
    scheduling_parameters.sched_priority = 2;
    CHECK(sched_setscheduler(child, SCHED_FIFO, &scheduling_parameters) != -1);

    /* ADDED: forking another time to create the "printer" */
    CHECK((child = fork()) != -1);

    if (!child) // child (T1) - priorities are inherited, which means that
    {           // this process' priority is equal to the father's (3)
      /* Set capacity to 2 */
      C = 2;

      /* Creating the timer */
      CHECK(timer_create(CLOCK_REALTIME, &sevp, &timer_parent) != -1);

      /* Setting the timer to an absolute time (ref + 1 second) with period=4 */
      time_parameters.it_value.tv_sec = ref.tv_sec + 1;
      time_parameters.it_value.tv_nsec = ref.tv_nsec;
      time_parameters.it_interval.tv_sec = 0;
      time_parameters.it_interval.tv_nsec = 4 * 1e6;

      CHECK(timer_settime(timer_parent, TIMER_ABSTIME, &time_parameters, NULL)
                  != -1);
    }
    else // parent (printer)
    {
      alarm(4); // the other ones will reach the other alarm(), but this one
                // requires another one since there is an infinite loop
      char buffer[MSGSIZE]; // where you receive the strings
      buffer[0] = 0; // reset the string

      while (1)
      {
          CHECK((mq_receive(queue, buffer, MSGSIZE, NULL)) != -1);
          printf("%s", buffer);
          buffer[0] = 0; // reset the string
      }
    }
  }
  alarm(4);

  /* Indefinitely wait for signals */
  while (1)
    sigsuspend(&block);

  exit(EXIT_SUCCESS);
}
