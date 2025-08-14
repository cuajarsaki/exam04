#include <unistd.h>
#include <stdlib.h>

int ft_popen(const char *file, char *const argv[], char type)
{
    int fd[2];
    pid_t pid;

    if (!file || !argv || (type != 'r' && type != 'w'))
        return (-1);

    if (pipe(fd) == -1)
        return (-1);

    pid = fork();
    if (pid == -1)
    {
        close(fd[0]);
        close(fd[1]);
        return (-1);
    }

    // child process
    if (pid == 0) 
    {
        // read mode: [1]をdup2して、[0]を返す
        if (type == 'r')
        {
            close(fd[0]);
            dup2(fd[1], STDOUT_FILENO);
            close(fd[1]);
            execvp(file, argv);
            exit(-1);
        }
        // write mode: その逆
        if (type == 'w')
        {
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            execvp(file, argv);
            exit(-1);
        }
    }
    
    // parent process
    if (type == 'r')
    {
        close(fd[1]);
        return (fd[0]);
    }
    if (type == 'w')
    {
        close(fd[0]);
        return (fd[1]);
    }

    return (-1); 
}
