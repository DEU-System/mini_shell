/* mini_shell.c */

/*
* 2022.11.27
* 동의대학교 컴퓨터소프트웨어공학과
* lab3 shell 만들기 팀 프로젝트
* 정민수 / 정현수
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

void ctrl_c(int sig){
    signal(sig, SIG_IGN);
    printf("Ctrl_C: 쉘 종료\n");
    exit(1);
}
void ctrl_z(int sig, int flag){
    signal(sig, SIG_IGN);
    printf(" 쉘 일시정지\n");
    printf(" fg 명령으로 재개 가능\n");
    raise(SIGSTOP);
    printf(" 쉘 재개..\n");
    signal(sig, ctrl_z);
}

void pwd_print(){
    char buf[1024];
    getcwd(buf, 1024);
    printf("%s", buf);
}

int getargs(char *cmd, char **argv) {
    int narg = 0;
    while (*cmd) {
        if (*cmd == ' ' || *cmd == '\t')
            *cmd++ = '\0';
        else {
            argv[narg++] = cmd++;
            while (*cmd != '\0' && *cmd != ' '&& *cmd != '\t')
                cmd++;
        }
    }
    argv[narg] = NULL;
    return narg;
}

void main() {
    char buf[256];
    char *argv[50];
    int narg;

    struct sigaction ctrlc_act;
    struct sigaction ctrlz_act;
    ctrlc_act.sa_handler = ctrl_c; 
    ctrlz_act.sa_handler = ctrl_z;
    
    //인터럽트 키
    sigaction(SIGINT, &ctrlc_act, NULL); 
    sigaction(SIGTSTP, &ctrlz_act, NULL);

    printf("Start mini shell\n");

    while (1) { /* 무한 루프
    	* 사용자로부터 명령어 입력을 받아들인다.
    	* 명령어 인자를 구분한다
    	* 자식 프로세스를 생성한다
    	* if(자식 프로세스) 명령어를 실행한다.
    	* else if(부모 프로세스) 자식 프로세스를 기다린다. */
    	
        pwd_print();
        printf(" : shell> ");
        gets(buf);
        narg = getargs(buf, argv);  //들어온 인자 갯수
        
        int opt = 0;  // 옵션: 백그라운드(&), 파일 재지향(>, <), 파이프(|)

        for (int i = 0; i < narg; i++) {

            if(!strcmp(argv[i], "exit")){ //exit를 입력받으면 쉘 종료
                printf("쉘을 종료합니다.\n");
                exit(1);
            }
            int opt = option(argv[i + 1]);    //-1 = 백그라운드(&), 1 = 파이프(|), 2 = 파일 재지향(<), 3 = 파일 재지향(>)
            if(opt == 1){
                run_pipe(i, argv);
                i += 2;
            }
            else{
                run(i, opt, argv);
            }
            if(opt > 1){
                i += 2;
            }
        } 

    }
}

int option(char *argv){  //실행 인자 포함 여부 확인 / 0=없음, -1 = 백그라운드(&), 1 = 파이프(|), 2 = 파일 재지향(<), 3 = 파일 재지향(>)
    int opt = 0;
    if(argv == NULL){
        return opt;
    }
    for(int i=0; argv[i] != NULL; i++){
        if (argv[i] == '&'){
            opt = -1;
            return opt;
        }
        if (argv[i] == '|'){
            opt = 1;
            return opt;
        }
        if (argv[i] == '<'){
            opt = 2;
            return opt;
        }
        if (argv[i] == '>'){
            opt = 3;
            return opt;
        }
    } 
    return opt;
}

void run(int i, int opt, char **argv){
    pid_t pid;
    int fd; // file descriptor
    char *buf[1024];
    int flags = O_RDWR | O_CREAT;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; /* == 0644 */
    memset(buf, 0, 1024);
    pid = fork();
    
    if (pid == 0){  //child
        //0 = 없음, -1 = 백그라운드(&), 1 = 파이프(|), 2 = 파일 재지향(<), 3 = 파일 재지향(>)
        if(opt == -1){ // 백그라운드(&)
            printf("%s 백그라운드로 실행\n", argv[i]);
            selectCmd(i, argv);
            exit(0);
        }
        else if(opt == 0){ // 없음
            selectCmd(i, argv);
            exit(0);
        }
        else if(opt == 2){ //파일 재지향(<)
            if ((fd = open(argv[i + 2], flags, mode)) == -1) {
                perror("open"); /* errno에 대응하는 메시지 출력됨*/
                exit(1);
            }
            if (dup2(fd, STDIN_FILENO) == -1) {
                perror("dup2"); /* errno에 대응하는 메시지 출력됨 */
                exit(1);
            }
            if (close(fd) == -1) {
                perror("close"); /* errno에 대응하는 메시지 출력됨*/
                exit(1);
            }
            // my_cat(argv[i+2]); // @@@@@@@@@@@@ 구현하고 주석 지워줘야함@@@@@@@@@@@@@
            selectCmd(i, argv);
            exit(0);
        }
        else if(opt == 3){ //파일 재지향(>)
            if ((fd = open(argv[i+2], flags, mode)) == -1) {
                perror("open"); /* errno에 대응하는 메시지 출력됨*/
                exit(1);
            }
            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("dup2"); /* errno에 대응하는 메시지 출력됨 */
                exit(1);
            }
            if (close(fd) == -1) {
                perror("close"); /* errno에 대응하는 메시지 출력됨*/
                exit(1);
            }
            selectCmd(i, argv);
            exit(0);
        }
    }
    else if (pid > 0){  //parent - 백그라운드 아닐 때만 기다림
        if(opt >= 0){ //백그라운드가 아닐 때
            wait(pid);
	    }
        if(!strcmp(argv[i], "cd")){ // 정민수
        
        }
    }
    else{
        perror("fork failed");
    }
}

void run_pipe(int i, char **argv){
}

void selectCmd(int i, char **argv){
    //argv 판별 후, 알맞은 명령 실행
    if(!strcmp(argv[i], "cat")){ //정현수

    }
    else if(!strcmp(argv[i], "ls")){ //정민수
    
    }
    else if(!strcmp(argv[i], "pwd")){ //정민수
    
    }
    else if(!strcmp(argv[i], "mkdir")){ //정민수

    }
    else if(!strcmp(argv[i], "rmdir")){ //정민수

    }
    else if(!strcmp(argv[i], "ln")){ //정현수

    }
    else if(!strcmp(argv[i], "cp")){ //정현수

    }
    else if(!strcmp(argv[i], "rm")){ //정현수

    }
    else if(!strcmp(argv[i], "mv")){ //정현수

    }
}

