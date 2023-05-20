#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */

void DieWithError(char *errorMessage); /* Error handling function */

enum REQUEST_CODE
{
    GET_WORK = 0,
    SEND_TASK = 1,
    SEND_CHECK = 2
};

enum RESPONSE_CODE
{
    UB = -1,
    NEW_TASK = 0,
    CHECK_TASK = 1,
    FIX_TASK = 2,
    FINISH = 3
};

enum STATUS
{
    NEW = -1,
    EXECUTING = 0,
    EXECUTED = 1,
    CHECKING = 2,
    WRONG = 3,
    RIGHT = 4
};

struct task
{
    int id;
    int executor_id;
    int checker_id;

    int status;
};

struct request
{
    int request_code;
    int programmer_id;

    struct task task;
};

struct response
{
    int response_code;
    struct task task;
};

int programmer_id;

void sendRequest(int sock, struct request *request)
{
    /* Send the current i to the server */
    if (send(sock, (struct request *)request, sizeof(*request), 0) < 0)
        DieWithError("send() bad");
    printf("Programmer #%d has sent his request = %d to server\n", programmer_id, request->request_code);
}

int main(int argc, char *argv[])
{
    int sock;                        /* Socket descriptor */
    struct sockaddr_in echoServAddr; /* Echo server address */
    unsigned short echoServPort;     /* Echo server port */
    char *servIP;                    /* Server IP address (dotted quad) */

    servIP = "127.0.0.1";

    programmer_id = -1;

    if (argc > 1)
    {
        programmer_id = atoi(argv[1]);
    }
    else
    {
        exit(1);
    }

    if ((argc < 3) || (argc > 4)) /* Test for correct number of arguments */
    {
        fprintf(stderr, "Usage: %s <programmer id> <Server IP> <Echo Port>\n",
                argv[0]);
        // exit(1);
    }
    else
    {
        servIP = argv[2]; /* First arg: server IP address (dotted quad) */
    }

    if (argc == 4)
        echoServPort = atoi(argv[3]); /* Use given port, if any */
    else
        echoServPort = 7004; // default

    /* Create a reliable, stream socket using TCP */
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed");

    /* Construct the server address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                /* Internet address family */
    echoServAddr.sin_addr.s_addr = inet_addr(servIP); /* Server IP address */
    echoServAddr.sin_port = htons(echoServPort);      /* Server port */

    /* Establish the connection to the echo server */
    if (connect(sock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0)
        DieWithError("connect() failed");

    struct task task = {-1, -1, -1, -1};
    struct request request = {GET_WORK, programmer_id, task};

    while (1)
    {

        sendRequest(sock, &request);

        struct response response = {-1, -1, -1, -1};
        if (recv(sock, &response, sizeof(response), 0) < 0)
        {
            DieWithError("recv() bad");
        }
        printf("Programmer #%d has got the responce = %d from server\n", programmer_id, response.response_code);

        if (response.response_code == FINISH)
        {
            break;
        }

        switch (response.response_code)
        {
        case UB:
            break;
        case NEW_TASK:
            sleep(1);
            request.task = response.task;
            request.request_code = SEND_TASK;
            break;
        case CHECK_TASK:
            sleep(1);

            // imitating check
            int8_t result = rand() % 2;
            printf("the result of checking task with id = %d is %d", response.task.id, result);
            response.task.status = result == 0 ? WRONG : RIGHT;

            request.task = response.task;
            request.request_code = SEND_CHECK;
            break;
        case FIX_TASK:
            sleep(1);
            request.task = response.task;
            request.request_code = SEND_TASK;
            break;
        default:
            request.request_code = GET_WORK;
            break;
        }
        sleep(5);
    }

    close(sock);
    exit(0);
}

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}
