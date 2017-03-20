#include <unistd.h>
#include <mqueue.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#define MQ_NAME "/myqueue"
#define MQ_NAME2 "/myqueue2"

pid_t child_pid;
size_t count;
size_t size;
mqd_t mq1, mq2;

void exit_program()
{
  mq_close(mq1);
  mq_close(mq2);
  printf("%lu messages reçus\n", count);
	printf("Débit : %.2f Mo/s\n", (float) (4*size*count/60)/1000000);
  kill(child_pid, SIGINT);
  exit(0);
}

int main(int argc, char ** argv)
{
  if(argc != 2)
  {
    fprintf(stderr, "Argument\n");
    exit(1);
  }

  count = 0;

  mq_unlink(MQ_NAME);
  mq_unlink(MQ_NAME2);

  struct mq_attr mq_attr1, mq_attr2;
  struct sigaction sa;
  size = atoi(argv[1]);

  mq_attr1.mq_maxmsg = 1;
  mq_attr1.mq_msgsize = size;
  mq_attr1.mq_flags = 0;
  mq_attr2.mq_maxmsg = 1;
  mq_attr2.mq_msgsize = size;
  mq_attr2.mq_flags = 0;

  if((mq1 = mq_open(MQ_NAME, O_CREAT|O_RDWR,
     S_IRWXU, &mq_attr1)) == (mqd_t)-1)
  {
    perror("mq_open");
    exit(1);
  }

  if((mq2 = mq_open(MQ_NAME2, O_CREAT|O_RDWR,
     S_IRWXU, &mq_attr2)) == (mqd_t)-1)
  {
    perror("mq_open");
    exit(1);
  }

  sa.sa_handler = exit_program;
  sigemptyset(&sa.sa_mask);
  if(sigaction(SIGALRM, &sa, NULL) < 0)
  {
    perror("sigaction");
    exit(1);
  }

  char buf[size];
  memset(&buf, 0, size);

  child_pid = fork();

  if(child_pid != 0)
  {
    alarm(60);

    while(1)
    {
      if(mq_send(mq1, buf, size, 0) == -1)
      {
        perror("mq_send_parent");
        exit(1);
      }

      if(mq_receive(mq2, buf, size, NULL) == -1)
      {
        perror("mq_receive_parent");
        exit(1);
      }

      count++;
    }
  }
  else
  {
    while(1)
    {
      if(mq_receive(mq1, buf, size, NULL) == -1)
      {
        perror("mq_receive_child");
        exit(1);
      }

      if(mq_send(mq2, buf, size, 0) == -1)
      {
        perror("mq_send_child");
        exit(1);
      }
    }
  }

  return -1;
}
