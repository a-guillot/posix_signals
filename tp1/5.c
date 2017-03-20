#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

size_t count_send;
pid_t pid;

void kill_child()
{
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
	sa2.sa_handler = kill_child; //Handler de l'interruption
	sigfillset(&sa2.sa_mask);
	sa2.sa_flags = 0;
	if (sigaction(SIGUSR1, &sa2, NULL)) //Met en place l'interruption pour SIGUSR2
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
		while(1)
		{
			if(sigqueue(pid,SIGRTMIN,sval) == -1)
			{
				perror("sigqueue");
				printf("Taille de la queue : %lu\n", count_send);
				printf("Valeur de _SC_SIGQUEUE_MAX : %ld\n", sysconf(_SC_SIGQUEUE_MAX));
				kill(pid,SIGUSR1);
				wait(NULL);
				exit(0);
			}
			count_send++;
		}
	}
	else
	{
		while(1)
		{
			
		}
	}

	return -1;
}