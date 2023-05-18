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

struct task
{
    int id;
    char *executor_ip;
    char *checker_ip;
};

struct pulls_t
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
    if ((client_socket = accept(service_socket, (struct sockaddr *)&client_addr, &addr_size)) < 0)
    {
        perror("[-]\tAccept error");
        exit(1);
    }
    printf("[+]\tClient connected\n");

    printf("Handling client %s\n", inet_ntoa(client_addr.sin_addr));

    return client_socket;
}

void executeTask(struct task *task, const char *client_ip)
{
    if (task->executor_ip == NULL)
    {
        // first time exe
        task->executor_ip = strdup(client_ip);
        printf("Client with ip: %s has got task with id = %d for execution\n", client_ip, task->id);
    }
    else if (strcmp(cur_task.executor_ip, client_ip) == 0)
    {
        // same ip, fixing
        printf("Client with ip: %s fixing task with id = %d\n", client_ip, task->id);
    }
    else
    {
        // trying to do someone's task
        return;
    }
}

void completeTaskExecution(struct task *task, struct pulls_t *pulls, const char *client_ip)
{
    int i = task->id;
    pulls->check[i] = *task;
    printf("Client with ip: %s has finished executing task with id = %d\n", client_ip, task->id);
}

void checkTask(struct task *task, struct pulls_t *pulls, const char *client_ip)
{
    task->checker_ip = strdup(client_ip);
    printf("Client with ip: %s has got task with id = %d for checking\n", client_ip, task->id);
}

void completeTaskChecking(struct task *task, struct pulls_t *pulls, int *result, const char *client_ip)
{
    if (*result == 0)
    {
        int i = task->id;
        pulls->complete[i] = *task;
        printf("Client with ip: %s has finished checking task with id = %d: client with ip: %s executed task correctly!\n", client_ip, task->id, task->executor_ip);
    }
    else
    {
        printf("Client with ip: %s has finished checking task with id = %d: client with ip: %s executed task incorrectly!!! need to be reexecuted\n", client_ip, task->id, task->executor_ip);
    }
}

void getTaskResult(struct task *task, struct pulls_t *pulls, const char *client_ip)
{
    int i = task->id;
    if (pulls->complete[i] == NULL)
    {
        printf("Client with ip: %s has not completed task with id = %d yet\n", client_ip, task->id);
    }
    else
    {
        printf("Client with ip: %s has alredy completed task with id = %d\n", client_ip, task->id);
    }
}

void handleClientRequest(unsigned short request, const char *client_ip, struct pulls_t *pulls)
{
    int i = -1;
    int result = -1; // Initialize result to -1

    switch (request)
    {
    case 0: // Get task for execution
        struct task cur_task = NULL;
        for (i = 0; i < MAX_TASK_COUNT; ++i)
        {
            cur_task = pulls->execution[i];
            // под это условие попадают как программы, которые ещё не выполнялись,
            // так и программы, которые были выполнены с ошибками (т.к. в них не обновляется checker_ip)
            if (cur_task != NULL && cur_task.checker_ip == NULL)
            {
                printf("[switch-if-0]__have_found_task_for_exe__\n");
                break;
            }
        }
        if (cur_task == NULL)
        {
            printf("No task for execution found\n");
            return;
        }
        executeTask(&cur_task, client_ip);
        break;

    case 1: // Return result of task execution
        struct task cur_task = NULL;
        for (i = 0; i < MAX_TASK_COUNT; ++i)
        {
            cur_task = pulls->execution[i];
            // забираем первую задачу с совпадающим айпи клиента-программиста
            if (cur_task != NULL && cur_task.executor_ip && strcmp(cur_task.executor_ip, client_ip) == 0)
            {
                printf("[switch-if-1]__have_found_exe_task__\n");
                break;
            }
        }
        if (cur_task == NULL)
        {
            printf("No task for finish execution found\n");
            return;
        }
        completeTaskExecution(&cur_task, pulls, client_ip);
        break;

    case 2: // Get task for checking
        struct task cur_task = NULL;
        for (i = 0; i < MAX_TASK_COUNT; ++i)
        {
            cur_task = pulls->execution[i];
            // можно назначать и задачи, у которых ещё не наметился исполнитель
            if (cur_task != NULL && cur_task.checker_ip == NULL)
            {
                printf("[switch-if-2]__have_found_task_for_che__\n");
                break;
            }
        }
        if (cur_task == NULL)
        {
            printf("No task for check found\n");
            return;
        }
        checkTask(&cur_task, pulls, client_ip);
        break;

    case 3: // Return result of task checking
        struct task cur_task = NULL;
        for (i = 0; i < MAX_TASK_COUNT; ++i)
        {
            cur_task = pulls->execution[i];
            // такая задача уже должна быть выполнена (чтобы быть проверенной)
            if (cur_task != NULL && cur_task.executor_ip != NULL && cur_task.checker_ip != NULL && strcmp(cur_task.checker_ip, client_ip) == 0)
            {
                printf("[switch-if-3]__have_found_che_task__\n");
                break;
            }
        }
        if (cur_task == NULL)
        {
            printf("No task for finish check found\n");
            return;
        }
        result = rand() % 2; // race condition?..
        completeTaskChecking(&cur_task, pulls, &result, client_ip);
        result = -1;
        break;

    case 4: // Get result of task execution check
        struct task cur_task = NULL;
        for (i = 0; i < MAX_TASK_COUNT; ++i)
        {
            cur_task = pulls->execution[i];
            // такая задача уже должна быть выполнена и проверена хотя бы раз, получает результат исполнитель задачи
            if (cur_task != NULL && cur_task.executor_ip != NULL && cur_task.checker_ip != NULL && strcmp(cur_task.executor_ip, client_ip) == 0)
            {
                printf("[switch-if-4]__have_found_task__\n");
                break;
            }
        }
        if (cur_task == NULL)
        {
            printf("No task result found\n");
            return;
        }
        getTaskResult(&cur_task, pulls, client_ip);
        break;
    }
}

int main(int argc, char *argv[])
{
    int tasks_count = MAX_TASK_COUNT;
    char *ip = "127.0.0.1";
    unsigned short port = 5000;

    if (argc > 1)
    {
        tasks_count = atoi(argv[1]); // check if argv[1] can be converted to int!!! // it is difficult.......
        // in order to have tasks_count lower (or equal) than max
        tasks_count = (tasks_count > MAX_TASK_COUNT) ? MAX_TASK_COUNT : tasks_count;
    }

    // command line input:
    // tasks_count
    // programmer1_ip, programmer1_port
    // programmer2_ip, programmer2_port
    // programmer3_ip, programmer3_port

    int server_socket, client_socket;
    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE]; // maybe for messages?
    unsigned short request;

    struct pulls_t task_pulls = {0};
    for (int i = 0; i < tasks_count; ++i)
    {
        struct task task = {.id = i};
        task_pulls.execution[i] = task;
    }

    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket < 0)
    {
        perror("[-]\tSocket error");
        exit(1);
    }
    printf("[+]\tTCP server socket has been created\n");

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
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

        bzero(buffer, BUFFER_SIZE);
        recv(client_socket, &request, sizeof(request), 0);
        printf("Received request %hu from client\n", request);

        handleClientRequest(request, inet_ntoa(server_addr.sin_addr), &task_pulls);

        close(client_socket);
        printf("[+]\tClient disconnected.\n\n");
    }

    return 0;
}
