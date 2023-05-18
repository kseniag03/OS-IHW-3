#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    char *ip = "127.0.0.1";
    unsigned short port = 5000;

    int sock;
    struct sockaddr_in addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE];

    // input: ips, ports

    // sock = socket(AF_INET, SOCK_STREAM, 0);
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
    {
        perror("[-]\tSocket error");
        exit(1);
    }
    printf("[+]\tTCP client socket has been created\n");

    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port); // port;
    addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("[-]\tConnect error");
        exit(1);
    }
    printf("Connected to the server\n");

    bzero(buffer, BUFFER_SIZE);
    strcpy(buffer, "HELLO, THIS IS CLIENT.");
    printf("Client: %s\n", buffer);
    send(sock, buffer, strlen(buffer), 0); // mb need if

    bzero(buffer, BUFFER_SIZE);
    recv(sock, buffer, sizeof(buffer), 0);
    printf("Server says: %s\n", buffer);

    close(sock);
    printf("Disconnected from the server.\n");

    return 0;
}
