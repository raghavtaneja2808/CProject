#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#define MAX_STUDENTS 80
#include <time.h>
struct Student {
    int    sock;           /* this student's socket */
    int    roll;
    char   name[32];
    time_t joined_at;
    time_t last_heartbeat;
    long   idle_seconds;   /* latest value reported by the client */
    int    warned;         /* already warned for this idle period? */
    long   active_seconds; /* for the attendance report */
};
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
    struct Student students[MAX_STUDENTS];
    for (int i = 0; i < MAX_STUDENTS; i++){
    students[i].sock=-1;
    }
    printf("Waiting for students on port 5050\n");
    //this code was for single student
    // int student_sock=accept(server_sock,NULL,NULL);
    // if(student_sock<0){
    //     perror("accept");
    //     close(server_sock);
    //     return 1;
    // }
    // printf("A student connected\n");
    // char buf[256];
    // int n = recv(student_sock,buf,sizeof(buf)-1,0);
    // if(n>0){
    //     buf[n]='\0';
    //     printf("Student says %s\n",buf);
    // }
    
    while(1){
        fd_set readfs;
        FD_ZERO(&readfs);
        FD_SET(server_sock,&readfs);
        int max_sock=server_sock;
        for(int i =0;i<MAX_STUDENTS;i++){
            if(students[i].sock!=-1){
                FD_SET(students[i].sock,&readfs);
                if(students[i].sock>max_sock){
                    max_sock=students[i].sock;
                }
            }
        }
        int result = select(max_sock+1,&readfs,NULL,NULL,NULL);
        if(result<0){
            perror("select");
            break;
        }
        if(FD_ISSET(server_sock,&readfs)){
            int new_sock= accept(server_sock,NULL,NULL);
            for(int i=0;i<MAX_STUDENTS;i++){
                if(students[i].sock==-1){
                    students[i].sock=new_sock;
                    printf("Stock connected slot %d socket %d\n",i,new_sock);
                    break;
                }
            }
        }
    }
    close(server_sock);
    return 0;
}
