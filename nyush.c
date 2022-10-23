#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>                                                                                                                        

pid_t jobs_pid[101];
char *jobs_cmd[101];
int job_index = -1;
char *get_basename(){
    char *pathname = getcwd(NULL, 1000);
    int length = strlen(pathname);
    
    int index = length - 1;
    while(pathname[index] != '/')
        index --;
    char *basename = malloc((length - index + 1) * sizeof(char));
    if (index == 0)
        basename = pathname;
    else
        basename = & pathname[index + 1];
    return basename;
}

char **parser(char *command, int *ptr_len){
    //do I need to copy this???
    char cmd[strlen(command)];
    strcpy(cmd, command);
    char **ret = malloc((strlen(command) + 10) * sizeof(char *));
    char *ptr;
    int i = 0;
    const char *delim = " ";
    char *token = strtok_r(cmd, delim, &ptr);
    do {
        ret[i] = malloc((strlen(token) + 1) * sizeof(char));
        strcpy(ret[i ++], token);
    } while((token = strtok_r(NULL, delim, &ptr)));
    *ptr_len = i;
    return ret;
}

void print_command(char** command){
    int i = 0;
    while(command[i]){
        printf("[%d] %s\n", i + 1, command[i]);
        i ++;
    }
}

char **parse_command(char *command, ssize_t s, bool *flag, int *len){
    // check whether there is any input
    if(strcmp(command, "\n") == 0){
        *flag = false;
        return NULL;
    }
    command[strlen(command) - 1] = '\0';
    // check whether pipe is used illegally
    if(command[0] == 124 || command[s - 2] == 124){
        *flag = false;
        fprintf(stderr, "Error: invalid command\n");
        return NULL;
    }
    char *ptr;
    char **cmd_arr = malloc((strlen(command) + 1) * sizeof(char *));
    const char *delim = "|\n";
    char *token = strtok_r(command, delim, &ptr);
    int i = 0;
    do {
        cmd_arr[i] = malloc((strlen(token) + 1) * sizeof(char));
        strcpy(cmd_arr[i ++], token);
    } while ((token = strtok_r(NULL, delim, &ptr)));
    cmd_arr[i] = NULL;
    *len = i;
    return cmd_arr;
}

bool cmp_buildin(char *str){
    return (strcmp(str, "cd") == 0 || strcmp(str, "fg") == 0
            || strcmp(str, "exit") == 0 || strcmp(str, "jobs") == 0);
}

bool cmp_redir(char *str){
    if(str == NULL)
        return true;
    if((strcmp(str, "<") == 0) || (strcmp(str, ">") == 0) || (strcmp(str, ">>") == 0))
        return true;
    return false;
}

bool check_cmd(char **cmd_arr, int len){
    // check for invalid command (input output redir & build in command)
    for(int i = 0; cmd_arr[i]; i ++){
        int cmd_len = 0;
        char **cmd = parser(cmd_arr[i], &cmd_len);
        bool buildin = cmp_buildin(cmd[0]);
        if(buildin && len != 1){
            fprintf(stderr, "Error: invalid command\n");
            return false;
        }

        // check whether input output directory is valid
        int count_in = 0;
        int count_out = 0;
        for(int j = 0; cmd[j]; j++){
            bool valid_in = true;
            bool valid_out = true;
            if(strcmp(cmd[j], "<<") == 0)
                valid_in = false;
            else if(strcmp(cmd[j], "<") == 0){
                count_in ++;
                valid_in =  i == 0 && j != 0 && cmd[j + 1] && cmp_redir(cmd[j + 2]) && (count_in <= 1);
            }
            else if(strcmp(cmd[j], ">") == 0 || strcmp(cmd[j], ">>") == 0){
                count_out ++;
                valid_out = !cmd_arr[i + 1] && j != 0 && cmd[j + 1] && cmp_redir(cmd[j + 2]) && (count_out <= 1);
            }
            else
                continue;
            if(!valid_in || !valid_out){
                fprintf(stderr, "Error: invalid command\n");
                return false;
            }
        }
    }
    return true;
}
void cd(char** command){
    if(!command[1] || command[2]){
        fprintf(stderr, "Error: invalid command\n");
        return;
    }
    int ret;
    if(command[1][0] == '/'){
        ret = chdir(command[1]);
    }
    else{
        char* path = getcwd(NULL, 1000);
        strcat(path, "/");
        strcat(path, command[1]);
        ret = chdir(path);
    }
    if(ret == -1)
        fprintf(stderr, "Error: invalid directory\n");
}

void my_exit(char **cmd){
    if(cmd[1]){
        fprintf(stderr, "Error: invalid command\n");
        return;
    }
    if(job_index >= 0){
        fprintf(stderr, "Error: there are suspended jobs\n");
        return;
    }
    kill(getpid(), SIGKILL);
    
}

void fg(char **cmd){
    if(!cmd[0] || cmd[2]){
        fprintf(stderr, "Error: invalid command\n");
        return;     
    }
    int index = atoi(cmd[1]) - 1;   
    if(index < 0 || index > job_index){
            fprintf(stderr, "Error: invalid job\n");
            return;
    }
    int pid = jobs_pid[index];
    char *command = jobs_cmd[index];
    while(jobs_pid[index + 1] && jobs_cmd[index + 1]){
        jobs_pid[index] = jobs_pid[index + 1];
        jobs_cmd[index] = jobs_cmd[index + 1];
        index ++;
    }
    jobs_pid[index] = -1;
    jobs_cmd[index] = NULL;
    job_index --;
    int status; 
    kill(pid, SIGCONT);
    waitpid(pid, &status, WUNTRACED);
    // add to jobs again if job is stopped
    if(WIFSTOPPED(status)){
        jobs_pid[++ job_index] = pid;
        jobs_cmd[job_index] = malloc((strlen(command) + 1) * sizeof(char));
        strcpy(jobs_cmd[job_index], command);
    }
}

void jobs(char **cmd){
    if(cmd[1])
        fprintf(stderr, "Error: invalid command\n");
    print_command(jobs_cmd);
}

void buildin(char** cmd){
    char *cmdname = cmd[0];
    if(strcmp(cmdname, "cd") == 0) cd(cmd);
    else if(strcmp(cmdname, "fg") == 0) fg(cmd);
    else if(strcmp(cmdname, "exit") == 0) my_exit(cmd);
    else jobs(cmd);
}

bool redir(char **cmd, int cmd_len){ // return false if an error exists
    int cut_index = cmd_len;
    for(int j = 0; cmd[j]; j++){
        if(strcmp(cmd[j], "<") == 0){
            if(access(cmd[j + 1], F_OK) == -1){
                fprintf(stderr,"Error: invalid file\n");
                return false;
            }
            int fd = open(cmd[j + 1], O_RDONLY, S_IRUSR|S_IWUSR);
            dup2(fd, 0);
            close(fd);
            if (j < cut_index)
                cut_index = j;
        }
        else if(strcmp(cmd[j], ">") == 0){
            int fd = open(cmd[j + 1], O_RDONLY | O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            dup2(fd, 1);
            close(fd);
            if (j < cut_index)
                cut_index = j;
        }
        else if(strcmp(cmd[j], ">>") == 0){
            int fd = open(cmd[j + 1], O_RDONLY | O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
            dup2(fd, 1);
            close(fd);
            if (j < cut_index)
                cut_index = j;
        }
        else
            continue;
    }
    cmd[cut_index] = NULL;
    return true;
}
void handler(){ //ignore the signal
}

int main(){
    while(true){
        signal(SIGINT, handler);
        signal(SIGQUIT, handler);
        signal(SIGTSTP, handler);
        bool flag = true;
        //print prompt
        printf("[nyush %s]$ ", get_basename());
        fflush(stdout);

        char *command = NULL;
        size_t length= 1000 * sizeof(char);
        ssize_t s = getline(&command, &length, stdin);

        // parse the command using the pipe symbol "|"
        int len; // length of command array or number of commands piped together
        char **cmd_arr = parse_command(command, s, &flag, &len);

        if(!flag) continue;
        char **ptr = cmd_arr;
        if(!check_cmd(ptr, len)) continue;

        int pipefd_in;
        for (int i = 0; cmd_arr[i] && flag; i++){ 
            int pipefd[2];   
            int cmd_len = 0;
            char **cmd = parser(cmd_arr[i], &cmd_len);
            if(cmp_buildin(cmd[0])){
                buildin(cmd);
                break;
            }
         
            if(i != len - 1){
                pipe(pipefd);
            }           

            int pid = fork();
            // child process
            if(pid == 0){
                if(i == 0 && i != len - 1){
                    close(pipefd[0]);
                    dup2(pipefd[1], 1);
                    close(pipefd[1]);
                } 
                if(i == len - 1 && i != 0){
                    dup2(pipefd_in, 0);
                    close(pipefd[0]);
                }
                if(i != 0 && i != len - 1){
                    dup2(pipefd_in, 0);
                    close(pipefd[0]);
                    dup2(pipefd[1], 1);
                    close(pipefd[1]);
                }

                flag = redir(cmd, cmd_len);
                if(flag){
                    execvp(cmd[0], cmd);      
                    fprintf(stderr, "Error: invalid program\n");
                    flag = false;
                    exit(-1);
                } 
                else{
                    flag = false;
                    exit(-1);
                }
            }   
            int status;
            waitpid(pid, &status, WUNTRACED);
            if(WIFSIGNALED(status))
                printf("\n");
            if(WIFSTOPPED(status)){
                jobs_pid[++ job_index] = pid;
                jobs_cmd[job_index] = malloc((strlen(command) + 1) * sizeof(char));
                strcpy(jobs_cmd[job_index], command);
            }
            if(i == 0 && i == len - 1)
                continue;
            if(i == len - 1){
                close(pipefd[0]);
            }
            else{
                close(pipefd[1]);
                pipefd_in = pipefd[0];
            }        
        }
    }   
}

