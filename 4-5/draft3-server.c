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

    /*
    char *executor_ip;
    char *checker_ip;
    */
    int executor_id;
    int checker_id;
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

void executeTask(struct task *task, const unsigned short programmer_id)
{
    if (task->executor_id == -1)
    {
        // first time exe
        task->executor_id = programmer_id;
        printf("Client with ip: %d has got task with id = %d for execution\n", programmer_id, task->id);
    }
    else if (task->executor_id == programmer_id)
    {
        // same ip, fixing
        printf("Client with ip: %d fixing task with id = %d\n", programmer_id, task->id);
    }
    else
    {
        // trying to do someone's task
        return;
    }
}

void completeTaskExecution(struct task *task, struct pulls_t *pulls, const unsigned short programmer_id)
{
    int i = task->id;
    pulls->check[i] = *task;
    printf("Client with ip: %d has finished executing task with id = %d\n", programmer_id, task->id);
}

void checkTask(struct task *task, struct pulls_t *pulls, const unsigned short programmer_id)
{
    task->checker_id = programmer_id;
    printf("Client with ip: %d has got task with id = %d for checking\n", programmer_id, task->id);
}

void completeTaskChecking(struct task *task, struct pulls_t *pulls, int *result, const unsigned short programmer_id)
{
    if (*result == 0)
    {
        int i = task->id;
        pulls->complete[i] = *task;
        printf("Client with ip: %d has finished checking task with id = %d: client with ip: %d executed task correctly!\n", programmer_id, task->id, task->executor_id);
    }
    else
    {
        printf("Client with ip: %d has finished checking task with id = %d: client with ip: %d executed task incorrectly!!! need to be reexecuted\n", programmer_id, task->id, task->executor_id);
    }
}

void getTaskResult(struct task *task, struct pulls_t *pulls, const unsigned short programmer_id)
{
    int i = task->id;
    if (pulls->complete[i].id == -1)
    {
        printf("Client with ip: %d has not completed task with id = %d yet\n", programmer_id, task->id);
    }
    else
    {
        printf("Client with ip: %d has alredy completed task with id = %d\n", programmer_id, task->id);
    }
}

void handleClientRequest(struct pulls_t *pulls, unsigned short request, const unsigned short programmer_id)
{
    int i = -1;
    int result = -1; // Initialize result to -1

    struct task cur_task;

    switch (request)
    {
    case 0: // Get task for execution
        cur_task = (struct task){-1, -1, -1};
        for (i = 0; i < MAX_TASK_COUNT; ++i)
        {
            cur_task = pulls->execution[i];
            // под это условие попадают как программы, которые ещё не выполнялись,
            // так и программы, которые были выполнены с ошибками (т.к. в них не обновляется checker_ip)
            if (cur_task.id != -1 && cur_task.checker_id == -1)
            {
                printf("[switch-if-0]__have_found_task_for_exe__\n");
                break;
            }
        }
        if (cur_task.id == -1)
        {
            printf("No task for execution found\n");
            return;
        }
        executeTask(&cur_task, programmer_id);
        break;

    case 1: // Return result of task execution
        cur_task = (struct task){-1, -1, -1};
        for (i = 0; i < MAX_TASK_COUNT; ++i)
        {
            cur_task = pulls->execution[i];
            // забираем первую задачу с совпадающим айпи клиента-программиста
            if (cur_task.id != -1 && cur_task.executor_id != -1 && cur_task.executor_id == programmer_id)
            {
                printf("[switch-if-1]__have_found_exe_task__\n");
                break;
            }
        }
        if (cur_task.id == -1)
        {
            printf("No task for finish execution found\n");
            return;
        }
        completeTaskExecution(&cur_task, pulls, programmer_id);
        break;

    case 2: // Get task for checking
        cur_task = (struct task){-1, -1, -1};
        for (i = 0; i < MAX_TASK_COUNT; ++i)
        {
            cur_task = pulls->execution[i];
            // можно назначать и задачи, у которых ещё не наметился исполнитель
            if (cur_task.id != -1 && cur_task.checker_id == -1)
            {
                printf("[switch-if-2]__have_found_task_for_che__\n");
                break;
            }
        }
        if (cur_task.id == -1)
        {
            printf("No task for check found\n");
            return;
        }
        checkTask(&cur_task, pulls, programmer_id);
        break;

    case 3: // Return result of task checking
        cur_task = (struct task){-1, -1, -1};
        for (i = 0; i < MAX_TASK_COUNT; ++i)
        {
            cur_task = pulls->execution[i];
            // такая задача уже должна быть выполнена (чтобы быть проверенной)
            if (cur_task.id != -1 && cur_task.executor_id != -1 && cur_task.checker_id != -1 && cur_task.checker_id == programmer_id)
            {
                printf("[switch-if-3]__have_found_che_task__\n");
                break;
            }
        }
        if (cur_task.id == -1)
        {
            printf("No task for finish check found\n");
            return;
        }
        result = rand() % 2; // race condition?..
        completeTaskChecking(&cur_task, pulls, &result, programmer_id);
        result = -1;
        break;

    case 4: // Get result of task execution check
        cur_task = (struct task){-1, -1, -1};
        for (i = 0; i < MAX_TASK_COUNT; ++i)
        {
            cur_task = pulls->execution[i];
            // такая задача уже должна быть выполнена и проверена хотя бы раз, получает результат исполнитель задачи
            if (cur_task.id != -1 && cur_task.executor_id != -1 && cur_task.checker_id != -1 && cur_task.executor_id == programmer_id)
            {
                printf("[switch-if-4]__have_found_task__\n");
                break;
            }
        }
        if (cur_task.id == -1)
        {
            printf("No task result found\n");
            return;
        }
        getTaskResult(&cur_task, pulls, programmer_id);
        break;
    }
}

int main(int argc, char *argv[])
{
    int tasks_count = MAX_TASK_COUNT;
    char *ip = "127.0.0.1";
    unsigned short port = 5566;

    // command line input:
    // tasks_count

    if (argc > 1)
    {
        tasks_count = atoi(argv[1]); // check if argv[1] can be converted to int!!! // it is difficult.......
        // in order to have tasks_count lower (or equal) than max
        tasks_count = (tasks_count > MAX_TASK_COUNT) ? MAX_TASK_COUNT : tasks_count;
    }

    int server_socket, client_socket;
    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE]; // maybe for messages?
    unsigned short programmer_id, request;

    struct pulls_t task_pulls = {0};
    for (int i = 0; i < tasks_count; ++i)
    {
        struct task task = {.id = i, .executor_id = -1, .checker_id = -1};
        task_pulls.execution[i] = task;
    }
    for (int i = tasks_count; i < MAX_TASK_COUNT; ++i)
    {
        struct task task = {-1, -1, -1};
        task_pulls.execution[i] = task;
    }
    for (int i = 0; i < MAX_TASK_COUNT; ++i)
    {
        struct task task = {-1, -1, -1};
        task_pulls.check[i] = task;
        task_pulls.complete[i] = task;
    }

    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket < 0)
    {
        perror("[-]\tSocket error");
        exit(1);
    }
    printf("[+]\tTCP server socket has been created (server)\n");

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

        recv(client_socket, &programmer_id, sizeof(programmer_id), 0);
        recv(client_socket, &request, sizeof(request), 0);
        printf("Received request %d from client\n", request);

        // handleClientRequest(request, inet_ntoa(server_addr.sin_addr), &task_pulls);
        handleClientRequest(&task_pulls, request, programmer_id);

        close(client_socket);
        printf("[+]\tClient disconnected.\n\n");
    }

    return 0;
}
