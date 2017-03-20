#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

size_t count;
pid_t pid;

void exit_program()
{
	printf("%lu signaux envoyés\n", count);
	printf("Débit : %.2f Mo/s\n", (float) (4*count/60)/1000000);
	kill(pid,SIGUSR2);
	wait(NULL);
	exit(0);
}

void count_sig()
{
	count++;
}

void kill_child()
{
	printf("%lu signaux reçus\n", count);
	exit(0);
}

int main()
{
	count = 0;

	struct sigaction sa;
	sa.sa_handler = count_sig; //Handler de l'interruption
	sigemptyset(&sa.sa_mask); //Set mis à 0
	sa.sa_flags = 0;
	if (sigaction(SIGUSR1, &sa, NULL)) //Met en place l'interruption pour SIGUSR1
	{
		perror("sigaction");
		exit(1); 
	}

	struct sigaction sa2;
	sa2.sa_handler = kill_child; //Handler de l'interruption
	sigemptyset(&sa2.sa_mask); //Set mis à 0
	sa2.sa_flags = 0;
	if (sigaction(SIGUSR2, &sa2, NULL)) //Met en place l'interruption pour SIGUSR2
	{
		perror("sigaction");
		exit(1); 
	}

	struct sigaction sa3;
	sa3.sa_handler = exit_program; //Handler de l'interruption
	sigemptyset(&sa3.sa_mask); //Set mis à 0
	sa3.sa_flags = 0;
	if (sigaction(SIGALRM, &sa3, NULL)) //Met en place l'interruption pour SIGALRM
	{
		perror("sigaction");
		exit(1); 
	}

	pid = fork();	

	if(pid) //pid = 0 pour le fils
	{
		alarm(6);
		while(1)
		{
			kill(pid,SIGUSR1);
			count++;
		}
	}
	else
	{
		sigset_t empty;
		sigemptyset(&empty);
		while(1)
			sigsuspend(&empty);
	}

	return -1;
}