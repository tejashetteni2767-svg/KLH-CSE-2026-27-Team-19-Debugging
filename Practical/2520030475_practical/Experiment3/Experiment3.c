#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int main()
{
    pid_t child1, child2, child3;
    int status;

    // Create Child 1
    child1 = fork();

    if (child1 == 0)
    {
        printf("Child 1: PID = %d\n", getpid());
        sleep(2);
        printf("Child 1 completed\n");
        exit(10);
    }

    // Create Child 2
    child2 = fork();

    if (child2 == 0)
    {
        printf("Child 2: PID = %d\n", getpid());
        sleep(4);
        printf("Child 2 completed\n");
        exit(20);
    }

    // Create Child 3
    child3 = fork();

    if (child3 == 0)
    {
        printf("Child 3: PID = %d\n", getpid());
        sleep(1);
        printf("Child 3 completed\n");
        exit(30);
    }

    // Parent process
    printf("\nParent PID = %d\n", getpid());
    printf("Child 1 PID = %d\n", child1);
    printf("Child 2 PID = %d\n", child2);
    printf("Child 3 PID = %d\n", child3);

    // wait() waits for any child
    pid_t finished = wait(&status);

    printf("\nwait() collected Child PID = %d\n", finished);

    // waitpid() waits for a specific child
    waitpid(child2, &status, 0);

    printf("waitpid() collected Child 2 PID = %d\n", child2);

    // Collect remaining child
    wait(NULL);

    printf("All child processes completed.\n");

    return 0;
}
