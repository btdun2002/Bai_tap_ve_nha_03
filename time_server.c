#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_FORMAT_LENGTH 10

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
        int client = accept(listener, NULL, NULL);
        printf("New client connected: %d\n", client);

        if (fork() == 0)
        {
            close(listener);
            char buf[MAX_FORMAT_LENGTH + 10];
            while (1)
            {
                int ret = recv(client, buf, sizeof(buf), 0);
                if (ret <= 0)
                {
                    break;
                }
                buf[strcspn(buf, "\n")] = '\0';

                // Kiểm tra lệnh client
                if (strncmp(buf, "GET_TIME", 8) == 0)
                {
                    // Lấy định dạng từ client
                    char *format = buf + 9;

                    // Lấy thời gian hiện tại
                    time_t t = time(NULL);
                    struct tm *tm = localtime(&t);

                    char time_str[20];
                    // Định dạng thời gian theo yêu cầu
                    if (strcmp(format, "dd/mm/yyyy") == 0)
                        strftime(time_str, sizeof(time_str), "%d/%m/%Y", tm);
                    else if (strcmp(format, "dd/mm/yy") == 0)
                        strftime(time_str, sizeof(time_str), "%d/%m/%y", tm);
                    else if (strcmp(format, "mm/dd/yyyy") == 0)
                        strftime(time_str, sizeof(time_str), "%m/%d/%Y", tm);
                    else if (strcmp(format, "mm/dd/yy") == 0)
                        strftime(time_str, sizeof(time_str), "%m/%d/%y", tm);
                    else
                    {
                        strcpy(time_str, "Invalid format");
                    }

                    // Gửi kết quả cho client
                    send(client, time_str, strlen(time_str), 0);
                    send(client, "\n", 1, 0);
                }
                else
                {
                    char *msg = "Invalid command";
                    send(client, msg, strlen(msg), 0);
                }
            }
            close(client);
            exit(0);
        }

        close(client);
    }

    close(listener);

    return 0;
}
