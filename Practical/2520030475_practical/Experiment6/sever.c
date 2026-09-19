#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define FIFO1 "client_to_server"
#define FIFO2 "server_to_client"

int main() {
    char buffer[100];

    mkfifo(FIFO1, 0666);
    mkfifo(FIFO2, 0666);

    printf("Server started. Waiting for client...\n");

    int fd_read = open(FIFO1, O_RDONLY);
    int fd_write = open(FIFO2, O_WRONLY);

    while (1) {
        memset(buffer, 0, sizeof(buffer));

        read(fd_read, buffer, sizeof(buffer));

        printf("Client: %s\n", buffer);

        if (strcmp(buffer, "exit\n") == 0 || strcmp(buffer, "exit") == 0) {
            strcpy(buffer, "Server shutting down.");
            write(fd_write, buffer, strlen(buffer) + 1);
            break;
        }

        char response[100];
        snprintf(response, sizeof(response), "Server received: %s", buffer);

        write(fd_write, response, strlen(response) + 1);
    }

    close(fd_read);
    close(fd_write);

    unlink(FIFO1);
    unlink(FIFO2);

    return 0;
}
