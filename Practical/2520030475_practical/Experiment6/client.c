#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define FIFO1 "client_to_server"
#define FIFO2 "server_to_client"

int main() {
    char buffer[100];
    char response[100];

    printf("Connecting to server...\n");

    int fd_write = open(FIFO1, O_WRONLY);
    int fd_read = open(FIFO2, O_RDONLY);

    while (1) {
        printf("Enter message: ");
        fgets(buffer, sizeof(buffer), stdin);

        write(fd_write, buffer, strlen(buffer) + 1);

        read(fd_read, response, sizeof(response));

        printf("Server: %s\n", response);

        if (strcmp(buffer, "exit\n") == 0 || strcmp(buffer, "exit") == 0)
            break;
    }

    close(fd_write);
    close(fd_read);

    return 0;
}
