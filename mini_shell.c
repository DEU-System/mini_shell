/* mini_shell.c */

/*
* 2022.11.27
* 동의대학교 컴퓨터소프트웨어공학과
* lab3 shell 만들기 팀 프로젝트
* 정민수 / 정현수
*/

#include <stdio.h> // C언어 표준 입출력에 대한 라이브러리
#include <stdlib.h> // 문자열 변환, 의사 난수 생성, 동적 메모리 관리 등을 위한 라이브러리
#include <unistd.h> // 표준 심볼 상수 및 자료형에 대한 함수를 가지고 있으며, 쉘 사용에 필요한 각종 명령어를 처리하기 위한 라이브러리
#include <sys/types.h> // C 언어 환경에서 여러 프로세스를 이용하기 위하여 프로세스를 대기시키는데 사용되는 라이브러리
#include <dirent.h> // gcc를 통해서 사용할 수 있는 디렉토리 연관 라이브러리
#include <sys/stat.h> // 파일 정보를 담고 있는 라이브러리
#include <fcntl.h> // 리눅스 및 유닉스 환경에서 파일을 관리하기 위하여 사용되는 라이브러리
#include <errno.h> // 프로그램 에러 발생에 대한 처리를 위한 라이브러리
#include <string.h> // 문자열을 처리할 수 있는 라이브러리
#include <signal.h> // 특정 프로세스의 종료나, 사용자의 인터럽트가 발생할 때 시그널을 보내 이를 처리해주기 위한 라이브러리

#define BUFSIZE 512 

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
    printf(" 쉘 재개\n");
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
    	* 명령어 인자를 구분한다. */
    	
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
            cmd_cat(argv[i+2]);
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
        if(!strcmp(argv[i], "cd")){
            if(argv[i+1] == NULL) {
                fprintf(stderr, "few argument\n");
            }
            else {
                cmd_cd(argv[i+1]);
            }
        }
    }
    else{
        perror("fork failed");
    }
}

void run_pipe(int i, char **argv){
    char buf[1024];
    int p[2];
    int pid;

    if (pipe(p) == -1) {
        perror("pipe call failed");
        exit(1);
    }

    pid = fork();
    

    if(pid == 0) { // 자식 프로세스
        close(p[0]);
        if(dup2(p[1], STDOUT_FILENO) == -1) {
            perror("dup2"); // errno에 대응하는 메시지 출력
            exit(1);
        }
        close(p[1]);
        selectCmd(i, argv);
        exit(0);
    }
    else if (pid > 0) {
        wait(pid);
        char *arg[1024];
        close(p[1]);
        sprintf(buf, "%d", p[0]);
        arg[0] = argv[i+2];
        arg[1] = buf;
        selectCmd(0, arg);
    }
    else {
        perror("fork failed");
    }
}

void your_cat(int target){
    char buffer[512];
    while (read (target, buffer, 512) > 0){
        printf("%s", buffer);
    }
}

void selectCmd(int i, char **argv){
    //argv 판별 후, 알맞은 명령 실행
    if(!strcmp(argv[i], "cat")){
	    if(argv[i+1] == NULL){
            fprintf(stderr, "A few argument..!\n");
        }
        
        if( (argv[i + 1][0] - 48) == 3 ) {
            your_cat(arg);
        }
        else{
            cmd_cat(argv[i+1]);
        }
    }
    else if(!strcmp(argv[i], "ls")){
        cmd_ls();
    }
    else if(!strcmp(argv[i], "pwd")){
        cmd_pwd();
    }
    else if(!strcmp(argv[i], "mkdir")){
        if(argv[i+1] == NULL) {
            fprintf(stderr, "few argument\n");
        }
        else {
            cmd_mkdir(argv[i+1]);
        }        
    }
    else if(!strcmp(argv[i], "rmdir")){
        if(argv[i+1] == NULL) {
            fprintf(stderr, "few argument\n");
        }
        else {
            cmd_rmdir(argv[i+1]);
        }
    }
    else if(!strcmp(argv[i], "ln")){
	    cmd_ln(argv[i+1], argv[i+2]);
    }
    else if(!strcmp(argv[i], "cp")){
	    cmd_cp(argv[i+1], argv[i+2]);
    }
    else if(!strcmp(argv[i], "rm")){
	    cmd_rm(argv[i+1]);
    }
    else if(!strcmp(argv[i], "mv")){
	    cmd_mv(argv[i+1], argv[i+2]);
    }
}

void cmd_cd(char *path) {
    if(chdir(path) < 0) {
        perror("chdir");
        exit(1);
    }
    else {
        printf("move to ..");
        cmd_pwd();
    }
}

void cmd_rmdir(char *argv) {
    if(rmdir(argv) < 0) {
        perror("rmdir");
    }
}

void cmd_mkdir(char *path) {
    if(mkdir(path, 0777) < 0) {
        perror("mkdir");
    }
}

void cmd_pwd() {
    char buf[1024];
    getcwd(buf, 1024);
    printf("%s\n", buf);
}

void cmd_ls() {
    DIR *pdir; 
    struct dirent *pde;
    int i = 0;

    if ( (pdir = opendir(".")) < 0) {
        perror("opendir");
        exit(1);
    }

    while((pde = readdir(pdir)) != NULL) {
        printf("%20s ", pde->d_name);

        if(++i % 3 == 0)
            printf("\n");
    }

    printf("\n");
    closedir(pdir);
}

void cmd_cat(char *argv){
	char buf[BUFSIZE];
	int fd;
	if((fd=open(argv,O_RDONLY)) == -1){ // 해당 파일(argv)를 읽기 전용으로 개방
		perror("open"); 
		exit(1);
	}
	while(read(fd, buf, BUFSIZE) >0){ // EOF까지 반복
		printf("%s", buf);
	}
}

void cmd_ln(char *argv1, char *argv2){
	if(link(argv1, argv2)<0){ //원본 argv1의 링크 argv2를 생성
		perror("link");
		exit(1);
	}
}

void cmd_cp(char *argv1, char *argv2){
	char buf[BUFSIZE];
	int argv1_fd, argv2_fd; // 파일 디스크립터
	ssize_t readCount; 
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; /* == 0644 */
	
	if((argv1_fd = open(argv1,O_RDONLY)) == -1){ // 해당 파일(argv1)를 읽기 전용으로 개방
		perror("open"); 
		exit(1);
	}
	if((argv2_fd = creat(argv2,mode)) == -1){ // 파일(argv2) 생성
		perror("creat"); 
		exit(1);
	}
	while((readCount = read(argv1_fd, buf, BUFSIZE)) > 0){ //문자를 buf에 저장
		write(argv2_fd, buf, readCount); // buf에 저장된 문자를 argv2에 씀
	}
	if(readCount < 0){
		perror("read");
        	exit(1);
	}
	close(argv1_fd);
	close(argv2_fd);	
}

void cmd_rm(char *argv){
	remove(argv); //한 파일을 시스템으로부터 제거하는 명령
}

void cmd_mv(char *argv, char *path){
	cmd_cp(argv,path); //파일을 path 경로에 복사
	cmd_rm(argv); //기존 파일을 삭제
}