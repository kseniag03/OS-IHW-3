#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

#define PROGRAMMERS_COUNT 3

int main(int argc, char *argv[])
{
    char *ips[] = {"127.0.0.1", "127.0.0.1", "127.0.0.1"};
    unsigned short ports[] = {5001, 5002, 5003};

    // command line input:
    // programmer1_ip, programmer1_port
    // programmer2_ip, programmer2_port
    // programmer3_ip, programmer3_port
    if (argc > 1)
    {
        ips[0] = argv[1];
    }
    if (argc > 2)
    {
        ports[0] = atoi(argv[2]);
    }
    if (argc > 3)
    {
        ips[1] = argv[3];
    }
    if (argc > 4)
    {
        ports[1] = atoi(argv[4]);
    }
    if (argc > 5)
    {
        ips[2] = argv[5];
    }
    if (argc > 6)
    {
        ports[2] = atoi(argv[6]);
    }

    int sock;
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
    {
        perror("[-]\tSocket error");
        exit(1);
    }
    printf("[+]\tTCP server socket has been created\n");

    for (int i = 0; i < PROGRAMMERS_COUNT; ++i)
    {
        struct sockaddr_in addr;
        socklen_t addr_size;

        memset(&addr, '\0', sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(ports[i]);
        addr.sin_addr.s_addr = inet_addr(ips[i]);

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            perror("[-]\tConnect error");
            exit(1);
        }
        printf("Connected to the server\n");

        for (int request = 0; request < 5; ++request)
        {
            if (send(sock, &request, sizeof(request), 0) < 0)
            {
                perror("[-]\tSend error");
                exit(1);
            }
            sleep(5);
        }
    }

    close(sock);
    printf("Disconnected from the server.\n");

    return 0;
}
