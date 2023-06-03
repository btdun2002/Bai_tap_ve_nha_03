#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_CLIENTS 64
#define MAX_USERNAME_LENGTH 32
#define MAX_PASSWORD_LENGTH 32

struct client_info
{
    int fd;
    char username[MAX_USERNAME_LENGTH];
    int authenticated;
};

void signalHandler(int signo)
{
    int pid = wait(NULL);
    printf("Child %d terminated.\n", pid);
}

int main()
{
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1)
    {
        perror("socket() failed");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)))
    {
        perror("bind() failed");
        return 1;
    }

    if (listen(listener, 5))
    {
        perror("listen() failed");
        return 1;
    }

    signal(SIGCHLD, signalHandler);

    while (1)
    {
        printf("Waiting for new client.\n");
        struct client_info client = {0};
        client.fd = accept(listener, NULL, NULL);
        if (client.fd == -1)
        {
            perror("accept() failed");
            continue;
        }
        printf("New client accepted: %d.\n", client.fd);
        if (fork() == 0)
        {
            close(listener);
            char buf[256];
            while (1)
            {
                int ret = recv(client.fd, buf, sizeof(buf), 0);
                if (ret <= 0)
                    break;
                buf[strcspn(buf, "\n")] = '\0';
                printf("Received from %d: %s\n", client.fd, buf);

                if (client.authenticated == 0)
                {
                    // Yêu cầu thông tin đăng nhập từ client
                    char *username = strtok(buf, " ");
                    char *password = strtok(NULL, " ");

                    if (username == NULL || password == NULL)
                    {
                        send(client.fd, "Invalid input format. Please send 'username password' again\n", strlen("Invalid input format. Please send 'username password' again\n"), 0);
                        close(client.fd);
                        continue;
                    }

                    FILE *fp = fopen("database.txt", "r");
                    if (fp == NULL)
                    {
                        perror("Failed to open database file");
                        send(client.fd, "Internal server error. Please try again later\n", strlen("Internal server error. Please try again later\n"), 0);
                        continue;
                    }

                    int authenticated = 0;
                    char line[256];
                    while (fgets(line, sizeof(line), fp) != NULL)
                    {
                        line[strcspn(line, "\n")] = '\0';
                        char db_username[MAX_USERNAME_LENGTH];
                        char db_password[MAX_PASSWORD_LENGTH];
                        sscanf(line, "%s %s", db_username, db_password);

                        if (strcmp(username, db_username) == 0 && strcmp(password, db_password) == 0)
                        {
                            authenticated = 1;
                            strcpy(client.username, username);
                            client.authenticated = 1;
                            break;
                        }
                    }

                    fclose(fp);

                    if (authenticated == 0)
                    {
                        send(client.fd, "Invalid username or password\n", strlen("Invalid username or password\n"), 0);
                        break;
                    }
                    else
                    {
                        send(client.fd, "Authentication successful. You are now logged in\n", strlen("Authentication successful. You are now logged in\n"), 0);
                        continue;
                    }
                }
                else
                {
                    // Client đã được xác thực, thực hiện lệnh và trả kết quả
                    FILE *fp = popen(buf, "r");
                    if (fp == NULL)
                    {
                        perror("Failed to execute command");
                        send(client.fd, "Failed to execute command. Please try again later\n", strlen("Failed to execute command. Please try again later\n"), 0);
                        continue;
                    }

                    char result[1024];
                    int bytesRead = fread(result, 1, sizeof(result) - 1, fp);
                    result[bytesRead] = '\0';
                    pclose(fp);

                    send(client.fd, result, strlen(result), 0);
                }
            }
            close(client.fd);
            exit(0);
        }

        close(client.fd);
    }

    close(listener);

    return 0;
}