#include <pthread.h>   /* for POSIX threads */
#include <semaphore.h> /* for unnamed "mutexes" */
#include <signal.h>    /* for handling SIGINT */
#include "common.h"

#define MAX_TASK_COUNT 10
#define MAX_PROGRAMMERS_CONNECTIONS 3
#define MAX_LOGGERS_CONNECTIONS 1 // 100? for several clients-retranslators support

void HandleTCPProgrammerClient(int clntSocket, int port);            /* TCP client handling function */
void HandleTCPLoggerClient(int clntSocket, int port);                /* TCP client handling function */
int CreateTCPServerSocket(unsigned short port, int max_connections); /* Create TCP server socket */
int AcceptTCPConnection(int servSock);                               /* Accept TCP connection request */

void *ThreadMain(void *arg);  /* Main program of a thread */
void *ThreadMain2(void *arg); /* Main program of a thread */

void printTasksInfo(); /* Print status info for logging */

/* Structure of arguments to pass to client thread */
struct ThreadArgs
{
    int clntSock; /* Socket descriptor for client */
    int port;     // listening port
};

struct task tasks[MAX_TASK_COUNT];
int tasks_count, complete_count = 0;

sem_t tasks_handling;
sem_t print;
sem_t messages_handling;

sem_t programmers;
sem_t loggers;

int servSock, servSock2;       /* Socket descriptor for server */
pthread_t threadID, threadID2; /* Thread ID from pthread_create() */

char buffer[BUFFER_SIZE];

struct message messages_pull[BUFFER_SIZE];
int messages_cnt = 0;

int programmers_port, loggers_port;

void closeAll()
{
    printf("\n\nFINISH USING SIGINT\n\n");
    printTasksInfo();

    sem_destroy(&tasks_handling);    // Уничтожение семафора
    sem_destroy(&print);             // Уничтожение семафора
    sem_destroy(&messages_handling); // Уничтожение семафора
    sem_destroy(&programmers);       // Уничтожение семафора
    sem_destroy(&loggers);           // Уничтожение семафора

    close(servSock);
    close(servSock2);
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
    pthread_kill(threadID2, SIGKILL);
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

    // initialize message_pull
    for (int i = 0; i < BUFFER_SIZE; ++i)
    {
        struct message message = {.text = "NULL"};
        messages_pull[i] = message;
    }
}

void *ThreadMain(void *threadArgs)
{
    sem_wait(&programmers);

    int clntSock; /* Socket descriptor for client connection */
    int port;

    /* Guarantees that thread resources are deallocated upon return */
    pthread_detach(pthread_self());

    /* Extract socket file descriptor from argument */
    clntSock = ((struct ThreadArgs *)threadArgs)->clntSock;
    port = ((struct ThreadArgs *)threadArgs)->port;
    free(threadArgs); /* Deallocate memory for argument */

    HandleTCPProgrammerClient(clntSock, port);

    sem_post(&programmers);

    return (NULL);
}

void *ThreadMain2(void *threadArgs)
{
    sem_wait(&loggers);

    int clntSock; /* Socket descriptor for client connection */
    int port;

    /* Guarantees that thread resources are deallocated upon return */
    pthread_detach(pthread_self());

    /* Extract socket file descriptor from argument */
    clntSock = ((struct ThreadArgs *)threadArgs)->clntSock;
    port = ((struct ThreadArgs *)threadArgs)->port;
    free(threadArgs); /* Deallocate memory for argument */

    HandleTCPLoggerClient(clntSock, port);

    sem_post(&loggers);

    return (NULL);
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

////// LOGGER LOGIC //////

void addLog(char *log)
{
    // critical section, access to messages pull
    sem_wait(&messages_handling);
    printf("%s", log);
    if (messages_cnt <= BUFFER_SIZE)
    {
        struct message new_message = {log};
        messages_pull[messages_cnt++] = new_message;
    }
    else
    {
        free(log);
        DieWithError("too many messages in pull");
    }
    // end of critical section
    sem_wait(&messages_handling);
}

////// END OF LOGGER LOGIC //////

//////  TASK LOGIC  //////

void printTasksInfo()
{
    // critical section, access to tasks array
    sem_wait(&print);
    for (int j = 0; j < tasks_count; ++j)
    {
        char *log = malloc(sizeof(char) * BUFFER_SIZE);
        sprintf(log, "task with id = %d and status = %d, executed by programmer #%d, checked by programmer %d\n", tasks[j].id, tasks[j].status, tasks[j].executor_id, tasks[j].checker_id);
        addLog(log);
    }
    // end of critical section
    sem_post(&print);
}

void getWork(struct response *response, int programmer_id)
{
    for (int i = 0; i < tasks_count; ++i)
    {
        if (tasks[i].status == NEW)
        {
            // found task for execution
            char *log = malloc(sizeof(char) * BUFFER_SIZE);
            sprintf(log, "programmer #%d has found task with id = %d for executing\n", programmer_id, tasks[i].id);
            addLog(log);

            response->response_code = NEW_TASK;
            tasks[i].executor_id = programmer_id;
            tasks[i].status = EXECUTING;
            response->task = tasks[i];
            return;
        }
        else if (tasks[i].status == FIX && tasks[i].executor_id == programmer_id)
        {
            // fix task
            char *log = malloc(sizeof(char) * BUFFER_SIZE);
            sprintf(log, "programmer #%d is fixing task with id = %d\n", programmer_id, tasks[i].id);
            addLog(log);

            response->response_code = NEW_TASK;
            tasks[i].status = EXECUTING;
            response->task = tasks[i];
            return;
        }
        else if (tasks[i].status == EXECUTED && tasks[i].executor_id != programmer_id)
        {
            // found task for check
            char *log = malloc(sizeof(char) * BUFFER_SIZE);
            sprintf(log, "programmer #%d has found task with id = %d for checking\n", programmer_id, tasks[i].id);
            addLog(log);

            response->response_code = CHECK_TASK;
            tasks[i].checker_id = programmer_id;
            tasks[i].status = CHECKING;
            response->task = tasks[i];
            return;
        }
        else if (tasks[i].executor_id == programmer_id && tasks[i].status == WRONG)
        {
            // found task for fix
            char *log = malloc(sizeof(char) * BUFFER_SIZE);
            sprintf(log, "programmer #%d need to fix his task with id = %d\n", programmer_id, tasks[i].id);
            addLog(log);

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

        char *log = malloc(sizeof(char) * BUFFER_SIZE);
        sprintf(log, "\n\n!!!!!!!!\tComplete count = %d\t!!!!!!!!!!\n\n", complete_count);
        addLog(log);
    }
}

int handleClientRequest(int clntSocket, struct request *request)
{

    struct task null_task = {-1, -1, -1, -1};
    struct response response = {-1, null_task};

    // critical section, access to tasks array
    sem_wait(&tasks_handling);

    if (complete_count == tasks_count)
    {
        response.response_code = FINISH;

        char *log = malloc(sizeof(char) * BUFFER_SIZE);
        sprintf(log, "\n\nFINISH\n\n");
        addLog(log);
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
    sem_post(&tasks_handling);

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

    char *log = malloc(sizeof(char) * BUFFER_SIZE);
    sprintf(log, "Server has received request = %d from programmer %d\n", request->request_code, request->programmer_id);
    addLog(log);
    // printTasksInfo();
}

int m_index = 0;

void HandleTCPProgrammerClient(int clntSocket, int port)
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

    close(clntSocket); /* Close client socket */
}

void HandleTCPLoggerClient(int clntSocket, int port)
{
    bzero(buffer, BUFFER_SIZE);
    recv(clntSocket, buffer, sizeof(buffer), 0);
    printf("Client says: %s\n", buffer);

    sem_wait(&messages_handling);

    bzero(buffer, BUFFER_SIZE);
    strcpy(buffer, messages_pull[m_index++].text);
    printf("Server: %s\n", buffer);
    send(clntSocket, buffer, strlen(buffer), 0);

    sem_post(&messages_handling);

    close(clntSocket); /* Close client socket */
}

int main(int argc, char *argv[])
{
    (void)signal(SIGINT, handleSigInt);

    servSock2 = -1;
    int clntSock;
    int clntSock2 = -1;
    unsigned short echoServPort;
    int echoServPort2 = -1;
    struct ThreadArgs *threadArgs, *threadArgs2; /* Pointer to argument structure for thread */

    sem_init(&tasks_handling, 0, 1);    // Инициализация семафора
    sem_init(&print, 0, 1);             // Инициализация семафора
    sem_init(&messages_handling, 0, 1); // Инициализация семафора
    sem_init(&programmers, 0, 1);       // Инициализация семафора
    sem_init(&loggers, 0, 1);           // Инициализация семафора

    echoServPort = 7004;

    if (argc < 2) /* Test for correct number of arguments */
    {
        fprintf(stderr, "Аргументы:  %s [SERVER PORT] [TASK_COUNT] [SERVER PORT 2]\n", argv[0]);
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
        // in order to have tasks_count lower (or equal) than max (and 2 and more)
        tasks_count = (tasks_count > MAX_TASK_COUNT || tasks_count < 2) ? MAX_TASK_COUNT : tasks_count;
    }
    initPulls();

    struct message *message1;
    message1 = malloc(sizeof(struct message));
    message1->text = "Start";

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
            if (pthread_create(&threadID2, NULL, ThreadMain2, (void *)threadArgs2) != 0)
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
}