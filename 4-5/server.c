#include <pthread.h>   /* for POSIX threads */
#include <semaphore.h> /* for unnamed "mutexes" */
#include "common.h"

#define MAX_TASK_COUNT 10
#define MAX_CLIENT_CONNECTIONS 3

void HandleTCPClient(int clntSocket);           /* TCP client handling function */
int CreateTCPServerSocket(unsigned short port); /* Create TCP server socket */
int AcceptTCPConnection(int servSock);          /* Accept TCP connection request */

void *ThreadMain(void *arg); /* Main program of a thread */

void printTasksInfo(); /* Print status info for logging */

/* Structure of arguments to pass to client thread */
struct ThreadArgs
{
    int clntSock; /* Socket descriptor for client */
};

struct task tasks[MAX_TASK_COUNT];
int tasks_count, complete_count = 0;

sem_t sem;
sem_t print;

int servSock;       /* Socket descriptor for server */
pthread_t threadID; /* Thread ID from pthread_create() */

void closeAll()
{
    printf("\n\nFINISH USING SIGINT\n\n");
    printTasksInfo();

    sem_destroy(&sem);   // Уничтожение семафора
    sem_destroy(&print); // Уничтожение семафора

    close(servSock);
}

void handleSigInt(int sig)
{
    if (sig != SIGINT)
    {
        return;
    }
    closeAll();
    // kill thread
    pthread_kill(threadID, SIGKILL);
    exit(0);
}

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

    /* Guarantees that thread resources are deallocated upon return */
    pthread_detach(pthread_self());

    /* Extract socket file descriptor from argument */
    clntSock = ((struct ThreadArgs *)threadArgs)->clntSock;
    free(threadArgs); /* Deallocate memory for argument */

    HandleTCPClient(clntSock);

    return (NULL);
}

int CreateTCPServerSocket(unsigned short port)
{
    int sock;                        /* socket to create */
    struct sockaddr_in echoServAddr; /* Local address */

    /* Create socket for incoming connections */
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        DieWithError("socket() failed");
    }

    /* Construct local address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                /* Internet address family */
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    echoServAddr.sin_port = htons(port);              /* Local port */

    /* Bind to the local address */
    if (bind(sock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0)
    {
        DieWithError("bind() failed");
    }

    /* Mark the socket so it will listen for incoming connections */
    if (listen(sock, MAX_CLIENT_CONNECTIONS) < 0)
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

//////  TASK LOGIC  //////

void printTasksInfo()
{
    sem_wait(&print);
    for (int j = 0; j < tasks_count; ++j)
    {
        printf("task with id = %d and status = %d, executed by programmer #%d, checked by programmer %d\n", tasks[j].id, tasks[j].status, tasks[j].executor_id, tasks[j].checker_id);
    }
    sem_post(&print);
}

void getWork(struct response *response, int programmer_id)
{
    for (int i = 0; i < tasks_count; ++i)
    {
        if (tasks[i].status == NEW)
        {
            // found task for execution
            printf("programmer #%d has found task with id = %d for executing\n", programmer_id, tasks[i].id);
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
            // one possible case: programmer is trying to check its own executed task
            // printTasksInfo();
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

    // critical section, access to tasks array
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

    // end of critical section
    sem_post(&sem);

    // Send the response back to the client
    send(clntSocket, &response, sizeof(response), 0);

    return response.response_code;
}

//////  END OF TASK LOGIC  //////

void receiveRequest(int sock, struct request *request)
{
    /* Receive the current i to the server */
    if (recv(sock, (struct request *)request, sizeof(*request), 0) < 0)
    {
        DieWithError("recv() bad");
    }
    printf("Server has received request = %d from programmer %d\n", request->request_code, request->programmer_id);
}

void HandleTCPClient(int clntSocket)
{
    while (1) // until complete-pull is complete
    {
        struct request request = {-1, -1, -1};

        receiveRequest(clntSocket, &request);

        if (handleClientRequest(clntSocket, &request) == FINISH)
        {
            break;
        }
    }

    close(clntSocket); /* Close client socket */
}

int main(int argc, char *argv[])
{
    (void)signal(SIGINT, handleSigInt);

    int clntSock;                  /* Socket descriptor for client */
    unsigned short port;           /* Server port */
    struct ThreadArgs *threadArgs; /* Pointer to argument structure for thread */

    sem_init(&sem, 0, 1);   // Инициализация семафора
    sem_init(&print, 0, 1); // Инициализация семафора

    port = 7004; /* First arg:  local port */

    if (argc < 2) /* Test for correct number of arguments */
    {
        fprintf(stderr, "Аргументы:  %s [SERVER PORT] [TASK_COUNT]\n", argv[0]);
        // exit(1);
    }
    else
    {
        port = atoi(argv[1]); /* First arg:  local port */
    }

    tasks_count = MAX_TASK_COUNT;
    if (argc > 2)
    {
        tasks_count = atoi(argv[2]);
        // in order to have tasks_count lower (or equal) than max (and 2 and more)
        tasks_count = (tasks_count > MAX_TASK_COUNT || tasks_count < 2) ? MAX_TASK_COUNT : tasks_count;
    }
    initPulls();

    servSock = CreateTCPServerSocket(port);

    while (complete_count < tasks_count) /* run forever */
    {
        clntSock = AcceptTCPConnection(servSock);

        /* Create separate memory for client argument */
        if ((threadArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs))) == NULL)
            DieWithError("malloc() failed");
        threadArgs->clntSock = clntSock;

        /* Create client thread */
        if (pthread_create(&threadID, NULL, ThreadMain, (void *)threadArgs) != 0)
            DieWithError("pthread_create() failed");
        printf("with thread %ld\n", (long int)threadID);
    }
    /* NOT REACHED */
}
