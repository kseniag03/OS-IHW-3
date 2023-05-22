#include "common.h"

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    int sock;                        /* Socket descriptor */
    struct sockaddr_in echoServAddr; /* Echo server address */
    unsigned short echoServPort;     /* Echo server port */
    char *servIP;                    /* Server IP address (dotted quad) */

    servIP = "127.0.0.1";

    if ((argc < 2) || (argc > 3)) /* Test for correct number of arguments */
    {
        fprintf(stderr, "Usage: %s <Server IP> <Echo Port>\n",
                argv[0]);
        // exit(1);
    }
    else
    {
        servIP = argv[1]; /* First arg: server IP address (dotted quad) */
    }

    if (argc == 3)
        echoServPort = atoi(argv[2]); /* Use given port, if any */
    else
        echoServPort = 8000; // default

    /* Create a reliable, stream socket using TCP */
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed");

    /* Construct the server address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                /* Internet address family */
    echoServAddr.sin_addr.s_addr = inet_addr(servIP); /* Server IP address */
    echoServAddr.sin_port = htons(echoServPort);      /* Server port */

    char buffer[BUFFER_SIZE]; // buffer for tranfering messages

    /* Establish the connection to the echo server */
    if (connect(sock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0)
        DieWithError("connect() failed");

    while (1)
    {

        bzero(buffer, BUFFER_SIZE);
        strcpy(buffer, "HELLO, THIS IS CLIENT.");
        printf("Client: %s\n", buffer);
        send(sock, buffer, strlen(buffer), 0);

        bzero(buffer, BUFFER_SIZE);
        recv(sock, buffer, sizeof(buffer), 0);
        printf("Server says: %s\n", buffer);
    }

    close(sock);
    printf("Disconnected from the server.\n");
    exit(0);
}

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}
