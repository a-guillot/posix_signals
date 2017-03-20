#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

size_t count_send;
size_t count_rcv;
pid_t pid;

void exit_program()
{
	printf("%lu signaux envoyés par le père\n", count_send);
	printf("%lu signaux reçus par le père\n", count_rcv);
	printf("Débit : %.2f Mo/s\n", (float) (4*count_rcv/60)/1000000);
	kill(pid,SIGRTMIN+1);
	wait(NULL);
	exit(0);
}

void kill_child()
{
	printf("%lu signaux envoyés par le fils\n", count_send);
	printf("%lu signaux reçus par le fils\n", count_rcv);
	exit(0);
}

void nothing()
{

}

int main()
{
	sigset_t block;
	sigemptyset(&block);
	sigaddset(&block, SIGRTMIN);
	sigprocmask(SIG_BLOCK, &block, NULL);

	struct sigaction sa;
	sa.sa_sigaction = nothing; //Handler de l'interruption
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGRTMIN, &sa, NULL)) //Met en place l'interruption pour SIGUSR2
	{
		perror("sigaction");
		exit(1); 
	}

	struct sigaction sa2;
	sa2.sa_sigaction = kill_child; //Handler de l'interruption
	sigfillset(&sa2.sa_mask);
	sa2.sa_flags = SA_SIGINFO;
	if (sigaction(SIGRTMIN+1, &sa2, NULL)) //Met en place l'interruption pour SIGUSR2
	{
		perror("sigaction");
		exit(1); 
	}

	struct sigaction sa3;
	sa3.sa_sigaction = exit_program; //Handler de l'interruption
	sigfillset(&sa3.sa_mask);
	sa3.sa_flags = SA_SIGINFO;
	if (sigaction(SIGALRM, &sa3, NULL)) //Met en place l'interruption pour SIGALRM
	{
		perror("sigaction");
		exit(1); 
	}

	sigset_t empty;
	sigemptyset(&empty);

	union sigval sval;

	pid = fork();

	if(pid) //pid = 0 pour le fils
	{
		alarm(60);
		while(1)
		{
			sigqueue(pid,SIGRTMIN,sval);
			count_send++;
			sigsuspend(&empty);
			count_rcv++;
		}
	}
	else
	{
		while(1)
		{
			sigsuspend(&empty);
			count_rcv++;
			sigqueue(getppid(),SIGRTMIN,sval);
			count_send++;
		}
	}

	return -1;
}