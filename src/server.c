#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#define PORT 5050
#define MAX_STUDENTS 80
#define WARN_AFTER 480   
#define KICK_AFTER 600   

struct Student {
    int sock;                 // -1 means empty slot
    long long roll;           // 0 means not joined yet (long long: roll numbers are too big for int)
    char name[32];
    time_t camera_off_since;  // 0 means camera is on
    int warned;
};

struct Student students[MAX_STUDENTS];
int join_code;

void say(int sock, char *text) {
    send(sock, text, strlen(text), 0);
}

void log_event(int i, char *what) {
    time_t now = time(NULL);
    printf("Roll %lld %s: %s\n", students[i].roll, students[i].name, what);
    FILE *f = fopen("attendance.csv", "a");
    fprintf(f, "%lld,%s,%s,%s", students[i].roll, students[i].name, what, ctime(&now));
    fclose(f);
}

void remove_student(int i, char *reason) {
    if (students[i].roll != 0)
        log_event(i, reason);
    close(students[i].sock);
    students[i].sock = -1;
}

int main() {
    signal(SIGPIPE, SIG_IGN);  
    srand(time(NULL));
    join_code = 1000 + rand() % 9000;

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    int yes = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    listen(server_sock, 5);

    for (int i = 0; i < MAX_STUDENTS; i++)
        students[i].sock = -1;

    printf("ClassWatch running. JOIN CODE: %d\n", join_code);
    printf("Press Ctrl+C to end the class.\n\n");

    while (1) {
        fd_set readfs;
        FD_ZERO(&readfs);
        FD_SET(server_sock, &readfs);
        int max_sock = server_sock;
        for (int i = 0; i < MAX_STUDENTS; i++) {
            if (students[i].sock != -1) {
                FD_SET(students[i].sock, &readfs);
                if (students[i].sock > max_sock)
                    max_sock = students[i].sock;
            }
        }

        struct timeval timeout = {1, 0};
        select(max_sock + 1, &readfs, NULL, NULL, &timeout);

        if (FD_ISSET(server_sock, &readfs)) {
            int new_sock = accept(server_sock, NULL, NULL);
            for (int i = 0; i < MAX_STUDENTS; i++) {
                if (students[i].sock == -1) {
                    students[i].sock = new_sock;
                    students[i].roll = 0;
                    students[i].camera_off_since = 0;
                    students[i].warned = 0;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_STUDENTS; i++) {
            if (students[i].sock == -1 || !FD_ISSET(students[i].sock, &readfs))


            continue;

            char buf[100];
            int n = recv(students[i].sock, buf, sizeof(buf) - 1, 0);
            if (n <= 0) {
                remove_student(i, "left");
                continue;
            }
            buf[n] = '\0';

            if (strncmp(buf, "HELLO", 5) == 0) {
                int code;
                sscanf(buf, "HELLO %d %lld %31[^\n]", &code, &students[i].roll, students[i].name);
                if (code != join_code) {
                    say(students[i].sock, "DENIED Wrong join code\n");
                    students[i].roll = 0;
                    remove_student(i, "");
                } else {
                    log_event(i, "joined");
                    say(students[i].sock, "Welcome! You are in class.\n");
                }
            }
            else if (strncmp(buf, "BEAT", 4) == 0) {
                int camera;
                sscanf(buf, "BEAT %d", &camera);
                if (camera == 1) {
                    if (students[i].camera_off_since != 0)
                        log_event(i, "camera ON");
                    students[i].camera_off_since = 0;
                    students[i].warned = 0;
                } else if (students[i].camera_off_since == 0) {
                    log_event(i, "camera OFF");
                    students[i].camera_off_since = time(NULL);
                }
            }
        }

        for (int i = 0; i < MAX_STUDENTS; i++) {
            if (students[i].sock == -1 || students[i].roll == 0 || students[i].camera_off_since == 0)
                continue;

            long off = time(NULL) - students[i].camera_off_since;
            if (off >= KICK_AFTER) {
                say(students[i].sock, "KICK You were removed: camera off too long\n");
                remove_student(i, "kicked: camera off");
            } else if (off >= WARN_AFTER && students[i].warned == 0) {
                students[i].warned = 1;
                printf("WARNING: Roll %lld %s camera off for %ld sec\n", students[i].roll, students[i].name, off);
                say(students[i].sock, "WARNING: Turn on your camera or you will be removed!\n");
            }
        }
    }
}
