#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
int main(){
    int server_sock=socket(AF_INET,SOCK_STREAM,0);
    if(server_sock<0){
        perror("socket");
        return 1;
    }
    printf("success\n");
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_addr.s_addr=INADDR_ANY;
    addr.sin_port=htons(5050);
    addr.sin_family=AF_INET;
    if(bind(server_sock,(struct sockaddr *)&addr, sizeof(addr))<0){
        perror("bind");
        close(server_sock);
        return 1;
    }
    printf("Bound to port 5050\n");
    listen(server_sock,5);
    printf("Waiting for students on port 5050\n");
    int student_sock=accept(server_sock,NULL,NULL);
    if(student_sock<0){
        perror("accept");
        close(server_sock);
        return 1;
    }
    printf("A student connected\n");
    char buf[256];
    int n = recv(student_sock,buf,sizeof(buf)-1,0);
    if(n>0){
        buf[n]='\0';
        printf("Student says %s\n",buf);
    }
    close(student_sock);
    close(server_sock);
    return 0;
}
