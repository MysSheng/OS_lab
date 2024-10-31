#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../include/command.h"
#include "../include/builtin.h"

// ======================= requirement 2.3 =======================
/**
 * @brief 
 * Redirect command's stdin and stdout to the specified file descriptor
 * If you want to implement ( < , > ), use "in_file" and "out_file" included the cmd_node structure
 * If you want to implement ( | ), use "in" and "out" included the cmd_node structure.
 *
 * @param p cmd_node structure
 * 
 */
void redirection(struct cmd_node *p){
	//檢查p的in_file
	if (p->in_file) {
		//透過open回傳指向input_file的fd
		int in_fd = open(p->in_file, O_RDONLY);
		if (in_fd < 0) {
			perror("Error opening input file");
			exit(EXIT_FAILURE);
		}
		//將該檔案的fd指向標準輸入
		dup2(in_fd, STDIN_FILENO);
		close(in_fd);
	}
	//先確認有無I/O
	if (p->out_file) {
		//回傳指向out_file的fd
		int out_fd = creat(p->out_file, 0644);
		if (out_fd < 0) {
			perror("Error opening output file");
			exit(EXIT_FAILURE);
		}
		//fd指向標準輸出
		dup2(out_fd, STDOUT_FILENO);
		close(out_fd);
	}
}
// ===============================================================

// ======================= requirement 2.2 =======================
/**
 * @brief 
 * Execute external command
 * The external command is mainly divided into the following two steps:
 * 1. Call "fork()" to create child process
 * 2. Call "execvp()" to execute the corresponding executable file
 * @param p cmd_node structure
 * @return int 
 * Return execution status
 */
int spawn_proc(struct cmd_node *p)
{
	//先開一個子程序
	pid_t pid = fork();
	//子程序失敗，彈出訊息
	if (pid < 0) {
		perror("Fork failed");
		return -1;
	} else if (pid == 0) {
		//確認是否redirection
		if(p->in_file||p->out_file) {
			redirection(p);
		}
		//透過execvp替換掉當前的程序，讓新的程序接管。
		execvp(p->args[0], p->args);
		perror("execvp failed");  // Only reached if execvp fails
		exit(EXIT_FAILURE);
	} else { //父程序等待子程序完成
		int status;
		waitpid(pid, &status, 0);  // Wait for child process to complete
		if (WIFEXITED(status)) {
			return 1;
		} else {
			return -1;  // Return -1 if the child didn't exit normally
		}
	}
}
// ===============================================================


// ======================= requirement 2.4 =======================
/**
 * @brief 
 * Use "pipe()" to create a communication bridge between processes
 * Call "spawn_proc()" in order according to the number of cmd_node
 * @param cmd Command structure  
 * @return int
 * Return execution status 
 */
int fork_cmd_node(struct cmd *cmd)
{
	//給pipe分配通道
	int fd[2];
	pid_t pid;
	int last_status=0;
	struct cmd_node *current=cmd->head;

	while(current!=NULL) {
		pid=fork();
		if (pid < 0) {
			perror("Fork failed");
			exit(EXIT_FAILURE);
		}else if (pid == 0) {
			//子程序
			dup2(last_status, STDIN_FILENO);
			if(current->next!=NULL) {
				dup2(fd[1], STDOUT_FILENO);
			}
			close(fd[0]);
			execvp(current->args[0], current->args);
			exit(EXIT_FAILURE);
		}else {
			wait(NULL); 		/* Collect childs */
			close(fd[1]);
			last_status = fd[0];
			current=current->next;
		}
	}

}
// ===============================================================


void shell()
{
	while (1) {
		printf(">>> $ ");
		char *buffer = read_line();
		if (buffer == NULL)
			continue;

		struct cmd *cmd = split_line(buffer);
		
		int status = -1;
		// only a single command
		struct cmd_node *temp = cmd->head;
		
		if(temp->next == NULL){
			status = searchBuiltInCommand(temp);
			if (status != -1){
				int in = dup(STDIN_FILENO), out = dup(STDOUT_FILENO);
				if( in == -1 | out == -1)
					perror("dup");
				redirection(temp);
				status = execBuiltInCommand(status,temp);

				// recover shell stdin and stdout
				if (temp->in_file)  dup2(in, 0);
				if (temp->out_file){
					dup2(out, 1);
				}
				close(in);
				close(out);
			}
			else{
				//external command
				status = spawn_proc(cmd->head);
			}
		}
		// There are multiple commands ( | )
		else{
			
			status = fork_cmd_node(cmd);
		}
		// free space
		while (cmd->head) {
			
			struct cmd_node *temp = cmd->head;
      		cmd->head = cmd->head->next;
			free(temp->args);
   	    	free(temp);
   		}
		free(cmd);
		free(buffer);
		
		if (status == 0)
			break;
	}
}
