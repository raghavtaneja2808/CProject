#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <CoreMediaIO/CMIOHardware.h>
#define PORT 5050

int camera_is_on() {
    CMIOObjectPropertyAddress all_cameras = {kCMIOHardwarePropertyDevices,
        kCMIOObjectPropertyScopeGlobal, kCMIOObjectPropertyElementMain};
    CMIOObjectPropertyAddress is_running = {kCMIODevicePropertyDeviceIsRunningSomewhere,
        kCMIOObjectPropertyScopeWildcard, kCMIOObjectPropertyElementWildcard};

    CMIODeviceID cameras[10];
    UInt32 bytes = 0;
    CMIOObjectGetPropertyData(kCMIOObjectSystemObject, &all_cameras, 0, NULL, sizeof(cameras), &bytes, cameras);
    int count = bytes / sizeof(CMIODeviceID);

    for (int i = 0; i < count; i++) {
        UInt32 on = 0, size = 0;
        CMIOObjectGetPropertyData(cameras[i], &is_running, 0, NULL, sizeof(on), &size, &on);
        if (on)
            return 1;
    }
    return 0;
}

int main() {
    char ip[64], name[32];
    long long roll;
    int code;

    printf("Teacher's IP (127.0.0.1 if same laptop): ");
    scanf("%63s", ip);
    printf("Your name: ");
    scanf("%31s", name);
    printf("Roll number: ");
    scanf("%lld", &roll);
    printf("Join code: ");
    scanf("%d", &code);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    char msg[100];
    sprintf(msg, "HELLO %d %lld %s\n", code, roll, name);
    send(sock, msg, strlen(msg), 0);

    while (1) {
        sprintf(msg, "BEAT %d\n", camera_is_on());
        send(sock, msg, strlen(msg), 0);
        sleep(5);

        char buf[100];
        int n = recv(sock, buf, sizeof(buf) - 1, MSG_DONTWAIT);
        if (n == 0) {
            printf("Class ended.\n");
            break;
        }
        if (n > 0) {
            buf[n] = '\0';
            printf("%s", buf);
            if (strncmp(buf, "KICK", 4) == 0 || strncmp(buf, "DENIED", 6) == 0)
                break;
        }
    }
    close(sock);
    return 0;
}
