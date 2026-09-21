#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
int main(){
    int client_sock=socket(AF_INET,SOCK_STREAM,0);
    if(client_sock<0){
        perror("socket");
        return 1;
    }
    printf("success\n");
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_port=htons(5050);
    addr.sin_family=AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if(connect(client_sock, (struct sockaddr *)&addr, sizeof(addr))<0){
        perror("connect");
        close(client_sock);
        return 1;
    }
    printf("Connected to teacher!\n");
    char msg[]="HELLO 21 Rahul\n";
    send(client_sock, msg, strlen(msg), 0);
    close(client_sock);
    return 0;
}