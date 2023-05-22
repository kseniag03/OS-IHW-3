#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <pthread.h>    /* for POSIX threads */

#include <semaphore.h>

#define MAX_TASK_COUNT 10
#define MAX_PROGRAMMERS_CONNECTIONS 3
#define MAX_LOGGERS_CONNECTIONS 1 // 100? for several clients-retranslators support
#define BUFFER_SIZE 1024

void DieWithError(char *errorMessage);                               /* Error handling function */
void HandleTCPClient(int clntSocket, int count, int port);           /* TCP client handling function */
int CreateTCPServerSocket(unsigned short port, int max_connections); /* Create TCP server socket */
int AcceptTCPConnection(int servSock);                               /* Accept TCP connection request */

void *ThreadMain(void *arg); /* Main program of a thread */

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
    RIGHT = 4,
    FIX = 5
};

/* Structure of arguments to pass to client thread */
struct ThreadArgs
{
    int clntSock; /* Socket descriptor for client */
    int port;     // number of listening port
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

struct task tasks[MAX_TASK_COUNT];

int tasks_count, complete_count = 0;

sem_t sem;
sem_t print;

struct message
{
    char *text;
};

char buffer[BUFFER_SIZE];

int programmers_port, loggers_port;

void initPulls()
{
    // initialize tasks
    for (int i = 0; i < tasks_count; ++i)
    {
        struct task task = {.id = i, .executor_id = -1, .checker_id = -1, .status = -1};
        tasks[i] = task;
    }
}

void *ThreadMain(void *threadArgs)
{
    int clntSock; /* Socket descriptor for client connection */
    int port;

    /* Guarantees that thread resources are deallocated upon return */
    pthread_detach(pthread_self());

    /* Extract socket file descriptor from argument */
    clntSock = ((struct ThreadArgs *)threadArgs)->clntSock;
    port = ((struct ThreadArgs *)threadArgs)->port;
    free(threadArgs); /* Deallocate memory for argument */

    HandleTCPClient(clntSock, tasks_count, port);

    return (NULL);
}

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

int CreateTCPServerSocket(unsigned short port, int max_connections)
{
    int sock;                        /* socket to create */
    struct sockaddr_in echoServAddr; /* Local address */

    /* Create socket for incoming connections */
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed");

    /* Construct local address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                /* Internet address family */
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    echoServAddr.sin_port = htons(port);              /* Local port */

    /* Bind to the local address */
    if (bind(sock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0)
        DieWithError("bind() failed");

    /* Mark the socket so it will listen for incoming connections */
    if (listen(sock, max_connections) < 0)
    {
        DieWithError("listen() failed");
    }

    return sock;
}

int AcceptTCPConnection(int servSock)
{
    int clntSock;                    /* Socket descriptor for client */
    struct sockaddr_in echoClntAddr; /* Client address */
    unsigned int clntLen;            /* Length of client address data structure */

    /* Set the size of the in-out parameter */
    clntLen = sizeof(echoClntAddr);

    /* Wait for a client to connect */
    if ((clntSock = accept(servSock, (struct sockaddr *)&echoClntAddr, &clntLen)) < 0)
    {
        DieWithError("[-]\tAccept error");
    }
    printf("[+]\tClient connected\n");
    /* clntSock is connected to a client! */

    printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));

    return clntSock;
}

//////

void addLog(char *log)
{
    printf("%s", log);
    struct message *m = malloc(sizeof(struct message));
    m->text = log;
}

void printTasksInfo()
{
    sem_wait(&print);
    for (int j = 0; j < tasks_count; ++j)
    {
        addLog(sprintf("task with id = %d and status = %d, executed by programmer #%d, checked by programmer %d\n", tasks[j].id, tasks[j].status, tasks[j].executor_id, tasks[j].checker_id));
    }
    sem_post(&print);
}

void getWork(struct response *response, int programmer_id)
{

    for (int i = 0; i < tasks_count; ++i)
    {
        if (tasks[i].status == NEW) // tasks[i].executor_id == -1)
        {
            // found task for execution
            // printf("programmer #%d has found task with id = %d for executing\n", programmer_id, tasks[i].id);
            addLog(sprintf("programmer #%d has found task with id = %d for executing\n", programmer_id, tasks[i].id));
            response->response_code = NEW_TASK;
            tasks[i].executor_id = programmer_id;
            tasks[i].status = EXECUTING;
            response->task = tasks[i];
            return;
        }
        else if (tasks[i].status == FIX && tasks[i].executor_id == programmer_id)
        {
            // fix task
            printf("programmer #%d is fixing task with id = %d\n", programmer_id, tasks[i].id);
            response->response_code = NEW_TASK;
            tasks[i].status = EXECUTING;
            response->task = tasks[i];
            return;
        }
        else if (tasks[i].status == EXECUTED && tasks[i].executor_id != programmer_id)
        {
            // found task for check
            printf("programmer #%d has found task with id = %d for checking\n", programmer_id, tasks[i].id);
            response->response_code = CHECK_TASK;
            tasks[i].checker_id = programmer_id;
            tasks[i].status = CHECKING;
            response->task = tasks[i];
            return;
        }
        else if (tasks[i].executor_id == programmer_id && tasks[i].status == WRONG)
        {
            // found task for fix
            printf("programmer #%d need to fix his task with id = %d\n", programmer_id, tasks[i].id);
            response->response_code = FIX_TASK;
            tasks[i].status = FIX;
            response->task = tasks[i];
            return;
        }
        else
        {
            // ub?
            printTasksInfo();
        }
    }
    // no work found

    if (complete_count == tasks_count)
    {
        response->response_code = FINISH;
    }
}

void sendTask(struct task *task)
{
    // programmer has executed his task
    task->status = EXECUTED;
    tasks[task->id] = *task;
}

void sendCheckResult(struct task *task)
{
    tasks[task->id] = *task;
    if (task->status == RIGHT)
    {
        ++complete_count;
        printf("\n\n!!!!!!!!\tComplete count = %d\t!!!!!!!!!!\n\n", complete_count);
    }
}

int handleClientRequest(int clntSocket, struct request *request)
{

    struct task null_task = {-1, -1, -1, -1};
    struct response response = {-1, null_task};

    sem_wait(&sem);

    if (complete_count == tasks_count)
    {
        response.response_code = FINISH;

        printf("\n\nFINISH\n\n");
        printTasksInfo();
    }
    else
    {
        int programmer_id = request->programmer_id;
        struct task task = request->task;

        switch (request->request_code)
        {
        case 0: // get work
            getWork(&response, programmer_id);
            break;
        case 1: // send task
            sendTask(&task);
            getWork(&response, programmer_id);
            break;
        case 2: // send check result
            sendCheckResult(&task);
            getWork(&response, programmer_id);
            break;
        default: // ub
            break;
        }
    }

    sem_post(&sem);

    // Send the response back to the client
    send(clntSocket, &response, sizeof(response), 0);

    return response.response_code;
}

/////////////////////////

void receiveRequest(int sock, struct request *request)
{
    /* Receive the current i to the server */
    if (recv(sock, (struct request *)request, sizeof(*request), 0) < 0)
        DieWithError("recv() bad");
    // printf("Server has received request = %d from programmer %d\n", request->request_code, request->programmer_id);
}

void HandleTCPClient(int clntSocket, int count, int port)
{
    if (port == programmers_port)
    {
        // handle first client

        while (1) // until complete-pull is complete
        {
            struct request request = {-1, -1, -1};

            receiveRequest(clntSocket, &request);

            if (handleClientRequest(clntSocket, &request) == FINISH)
            {
                break;
            }
        }
    }
    else if (port == loggers_port)
    {
        // handle second client

        bzero(buffer, BUFFER_SIZE);
        recv(clntSocket, buffer, sizeof(buffer), 0);
        printf("Client says: %s\n", buffer);

        bzero(buffer, BUFFER_SIZE);

        struct message *m = TAILQ_FIRST(&head);
        if (m == NULL)
        {
            strcpy(buffer, "NULL");
        }
        else
        {

            strcpy(buffer, m->text);
            TAILQ_REMOVE(&head, m, entries);
        }
        printf("Server: %s\n", buffer);
        send(clntSocket, buffer, strlen(buffer), 0);
    }

    close(clntSocket); /* Close client socket */
}

int main(int argc, char *argv[])
{
    int servSock; /* Socket descriptor for server */
    int servSock2 = -1;
    int clntSock; /* Socket descriptor for client */
    int clntSock2 = -1;
    unsigned short echoServPort;
    int echoServPort2;                           /* Server port */
    pthread_t threadID, threadID2;               /* Thread ID from pthread_create() */
    struct ThreadArgs *threadArgs, *threadArgs2; /* Pointer to argument structure for thread */

    sem_init(&sem, 0, 1);   // Инициализация семафора
    sem_init(&print, 0, 1); // Инициализация семафора

    echoServPort = 7004; // atoi(argv[1]); /* First arg:  local port */
    echoServPort2 = -1;  // atoi(argv[1]); /* First arg:  local port */

    if (argc < 2) /* Test for correct number of arguments */
    {
        fprintf(stderr, "Usage:  %s <SERVER PORT> [TASK_COUNT] [SERVER PORT 2]\n", argv[0]);
        // exit(1);
    }
    else
    {
        echoServPort = atoi(argv[1]); /* First arg:  local port */
    }

    tasks_count = MAX_TASK_COUNT;
    if (argc > 2)
    {
        tasks_count = atoi(argv[2]);
        // in order to have tasks_count lower (or equal) than max
        tasks_count = (tasks_count > MAX_TASK_COUNT) ? MAX_TASK_COUNT : tasks_count;
    }
    initPulls();

    TAILQ_INIT(&head);

    struct message *message1;
    message1 = malloc(sizeof(struct message));
    message1->text = "Start";

    TAILQ_INSERT_TAIL(&head, message1, entries);

    servSock = CreateTCPServerSocket(echoServPort, MAX_PROGRAMMERS_CONNECTIONS);

    if (argc > 3)
    {
        echoServPort2 = atoi(argv[3]); /* local port for loggers */
    }

    if (echoServPort2 != -1 && echoServPort2 != echoServPort)
    {
        servSock2 = CreateTCPServerSocket(echoServPort2, MAX_LOGGERS_CONNECTIONS);
    }

    programmers_port = echoServPort;
    loggers_port = echoServPort2;

    while (complete_count < tasks_count) /* run forever */
    {
        clntSock = AcceptTCPConnection(servSock);

        if (servSock2 != -1)
        {
            clntSock2 = AcceptTCPConnection(servSock2);

            /* Create separate memory for client argument */
            if ((threadArgs2 = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs))) == NULL)
                DieWithError("malloc()2 failed");
            threadArgs2->clntSock = clntSock2;
            threadArgs2->port = echoServPort2;

            /* Create client thread for servSock2 */
            if (pthread_create(&threadID2, NULL, ThreadMain, (void *)threadArgs2) != 0)
                DieWithError("pthread_create()2 failed");
            printf("with thread %ld\n", (long int)threadID2);
        }

        /* Create separate memory for client argument */
        if ((threadArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs))) == NULL)
            DieWithError("malloc() failed");
        threadArgs->clntSock = clntSock;
        threadArgs->port = echoServPort;

        /* Create client thread */
        if (pthread_create(&threadID, NULL, ThreadMain, (void *)threadArgs) != 0)
            DieWithError("pthread_create() failed");
        printf("with thread %ld\n", (long int)threadID);
    }
    /* NOT REACHED */

    printf("reached!!!\n");

    printf("\n\nFINISH FINISH FINISH\n\n");
    printTasksInfo();

    sem_destroy(&sem);   // Уничтожение семафора
    sem_destroy(&print); // Уничтожение семафора

    close(servSock);
}