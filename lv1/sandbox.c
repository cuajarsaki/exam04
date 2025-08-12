#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

static volatile sig_atomic_t timeout_flag = 0;
void handle_alarm(int sig) { timeout_flag = 1; }

int sandbox(void (*f)(void), unsigned int timeout, bool verbose)
{
	struct sigaction sa = {.sa_handler = handle_alarm, .sa_flags = 0};
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, NULL);

	pid_t cpid = fork();
	if (cpid == -1)
		return -1;
	if (cpid == 0)
	{
		f();
		exit(0);
	}

	alarm(timeout);
	int status;

	while (waitpid(cpid, &status, 0) != cpid)
	{
		if (timeout_flag)
		{
			kill(cpid, SIGKILL);
			waitpid(cpid, &status, 0);
			if (verbose)
				printf("Bad function: timed out after %d seconds\n", timeout);
			return 0;
		}
	}

	if (WIFEXITED(status))
	{
		int code = WEXITSTATUS(status);
		if (verbose)
			printf(code ? "Bad function: exited with code %d\n" : "Nice function\n", code);
		return code ? 1 : 0;
	}

	if (verbose)
		printf("Bad function: %s\n", strsignal(WTERMSIG(status)));
	return 0;
}
