#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include <pthread.h>

#define BUFFER_SIZE 1024

#define MAX_TASK_COUNT 10

#define MAX_CLIENT_CONNECTIONS 3 // 4, 1 also for client-retranslator // 100? for several clients-retranslators support

struct Pulls
{
    int client_socket;
};

struct task
{
    int id;
    int executor_ip; // or char* ip?
    int checker_ip;
};

struct Pulls
{
    struct task execution[MAX_TASK_COUNT];
    struct task check[MAX_TASK_COUNT];
    struct task complete[MAX_TASK_COUNT];
};

int acceptTCPConnection(int service_socket)
{
    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t addr_size;

    addr_size = sizeof(client_addr);
    if (client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size) < 0)
    {
        perror("[-]\tAccept error");
        exit(1);
    }
    printf("[+]\tClient connected\n");

    printf("Handling client %s\n", inet_ntoa(client_addr.sin_addr));

    return client_socket;
}

int main(int argc, char *argv[])
{
    char *ip = "127.0.0.1";
    unsigned short port = 5000;

    int server_socket, client_socket;
    struct sockaddr_in server_addr;

    struct Pulls *tasks_pulls;
    char buffer[BUFFER_SIZE];

    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket < 0)
    {
        perror("[-]\tSocket error");
        exit(1);
    }
    printf("[+]\tTCP server socket has been created\n");

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port); // port;
    server_addr.sin_addr.s_addr = inet_addr(ip);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("[-]\tBind error");
        exit(1);
    }
    printf("[+]\tBind to the port number: %d\n", port);

    listen(server_socket, MAX_CLIENT_CONNECTIONS);
    printf("Listening...\n");

    while (1)
    {
        client_socket = acceptTCPConnection(server_socket);

        if ((tasks_pulls = (struct Pulls *)malloc(sizeof(struct Pulls))) == NULL)
        {
            perror("[-]\tMalloc error");
            exit(1);
        }
        tasks_pulls->client_socket = client_socket;
    }

    return 0;
}


/*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include <pthread.h>

#define BUFFER_SIZE 1024

#define MAX_TASK_COUNT 10

#define MAX_CLIENT_CONNECTIONS 3 // 4, 1 also for client-retranslator // 100? for several clients-retranslators support

//void *ThreadMain(void *arg);

struct Pulls
{
    int client_socket;
};

struct task
{
    int id;
    int executor_ip; // or char* ip?
    int checker_ip;
};

struct Pulls
{
    struct task execution[MAX_TASK_COUNT];
    struct task check[MAX_TASK_COUNT];
    struct task complete[MAX_TASK_COUNT];
};

int acceptTCPConnection(int service_socket)
{
    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t addr_size;

    addr_size = sizeof(client_addr);
    if (client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size) < 0)
    {
        perror("[-]\tAccept error");
        exit(1);
    }
    printf("[+]\tClient connected\n");

    printf("Handling client %s\n", inet_ntoa(client_addr.sin_addr));

    return client_socket;
}

int main(int argc, char *argv[])
{
    char *ip = "127.0.0.1";
    unsigned short port = 5000;

    int server_socket, client_socket;
    struct sockaddr_in server_addr; //, client_addr;
    // socklen_t addr_size;

    //pthread_t threadID;
    struct Pulls *tasks_pulls;

    char buffer[BUFFER_SIZE];

    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket < 0)
    {
        perror("[-]\tSocket error");
        exit(1);
    }
    printf("[+]\tTCP server socket has been created\n");

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port); // port;
    server_addr.sin_addr.s_addr = inet_addr(ip);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("[-]\tBind error");
        exit(1);
    }
    printf("[+]\tBind to the port number: %d\n", port);

    listen(server_socket, MAX_CLIENT_CONNECTIONS);
    printf("Listening...\n");

    while (1)
    {
        /*
        addr_size = sizeof(client_addr);
        if (client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size) < 0)
        {
            perror("[-]\tAccept error");
            exit(1);
        }
        printf("[+]\tClient connected\n");
        */
/*
        client_socket = acceptTCPConnection(server_socket);

        if ((tasks_pulls = (struct Pulls *)malloc(sizeof(struct Pulls))) == NULL)
        {
            perror("[-]\tMalloc error");
            exit(1);
        }
        tasks_pulls->client_socket = client_socket;
/*
        if (pthread_create(&threadID, NULL, ThreadMain, (void *)pulls) != 0)
        {
            perror("[-]\tPthread_create error");
            exit(1);
        }*/

        /*
        bzero(buffer, BUFFER_SIZE);
        recv(client_socket, buffer, sizeof(buffer), 0);
        printf("Client says: %s\n", buffer);

        bzero(buffer, BUFFER_SIZE);
        strcpy(buffer, "HI, THIS IS SERVER. HAVE A NICE DAY!!!");
        printf("Server: %s\n", buffer);
        send(client_socket, buffer, strlen(buffer), 0);

        close(client_socket);
        printf("[+]\tClient disconnected.\n\n");
        */
    }
/*
    return 0;
}
/*
void *ThreadMain(void *pulls)
{
    int client_socket;

    pthread_detach(pthread_self());

    client_socket = ((struct Pulls *)pulls)->client_socket;
    free(pulls);

    return (NULL);
}*/
