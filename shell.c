#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define MAX_COMMANDS 16

// Global variables for history
char history[MAX_COMMANDS][MAX_INPUT];
int history_count = 0;

// Function prototypes
void execute_command(char *args[], int arg_count);
void handle_redirection(char *args[], int arg_count);
void handle_piping(char *commands[], int cmd_count);
void execute_multiple_commands(char *commands[], int cmd_count, char *separator);
void add_to_history(char *command);
void print_history();
void handle_signal(int sig);

// Built-in commands
int is_builtin_command(char *args[], int arg_count) {
    if (strcmp(args[0], "cd") == 0) {
        if (arg_count < 2) {
            chdir(getenv("HOME"));
        } else {
            if (chdir(args[1]) != 0) {
                perror("cd");
            }
        }
        return 1;
    } else if (strcmp(args[0], "history") == 0) {
        print_history();
        return 1;
    } else if (strcmp(args[0], "exit") == 0) {
        exit(0);
    }
    return 0;
}

// Main shell loop
int main() {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];
    
    // Set up signal handling
    signal(SIGINT, handle_signal);
    
    while (1) {
        // Display prompt
        printf("sh> ");
        fflush(stdout);
        
        // Read input
        if (!fgets(input, MAX_INPUT, stdin)) {
            break; // Exit on EOF (Ctrl+D)
        }
        
        // Remove newline
        input[strcspn(input, "\n")] = '\0';
        
        // Skip empty input
        if (strlen(input) == 0) {
            continue;
        }
        
        // Add to history
        add_to_history(input);
        
        // Create a copy of input for parsing
        char input_copy[MAX_INPUT];
        strcpy(input_copy, input);
        
        // Check for pipes first
        if (strchr(input, '|') != NULL) {
            char *pipe_commands[MAX_COMMANDS];
            int pipe_count = 0;
            
            char *pipe_token = strtok(input_copy, "|");
            while (pipe_token != NULL && pipe_count < MAX_COMMANDS) {
                // Trim leading/trailing spaces
                while (*pipe_token == ' ') pipe_token++;
                
                pipe_commands[pipe_count++] = pipe_token;
                pipe_token = strtok(NULL, "|");
            }
            
            handle_piping(pipe_commands, pipe_count);
            continue;  // Skip the rest of the loop
        }
        
        // Check for multiple commands separated by ; or &&
        if (strstr(input, ";") || strstr(input, "&&")) {
            char *commands[MAX_COMMANDS];
            int cmd_count = 0;
            
            // Make new copy for tokenizing
            strcpy(input_copy, input);
            
            // Determine separator
            char *separator = strstr(input, "&&") ? "&&" : ";";
            
            char *cmd_token = strtok(input_copy, separator);
            while (cmd_token != NULL && cmd_count < MAX_COMMANDS) {
                commands[cmd_count++] = cmd_token;
                cmd_token = strtok(NULL, separator);
            }
            
            execute_multiple_commands(commands, cmd_count, separator);
            continue;  // Skip the rest of the loop
        }
        
        // Parse single command
        int arg_count = 0;
        char *token = strtok(input, " \t");
        while (token != NULL && arg_count < MAX_ARGS - 1) {
            args[arg_count++] = token;
            token = strtok(NULL, " \t");
        }
        args[arg_count] = NULL;
        
        // Execute single command
        if (!is_builtin_command(args, arg_count)) {
            execute_command(args, arg_count);
        }
    }
    
    return 0;
}
// Execute a single command
void execute_command(char *args[], int arg_count) {
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("fork");
        return;
    } else if (pid == 0) {
        // Child process
        handle_redirection(args, arg_count);
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        waitpid(pid, NULL, 0);
    }
}

// Handle I/O redirection
void handle_redirection(char *args[], int arg_count) {
    int i;
    int fd;
    
    for (i = 0; i < arg_count; i++) {
        if (strcmp(args[i], "<") == 0) {
            // Input redirection
            if (i + 1 < arg_count) {
                fd = open(args[i + 1], O_RDONLY);
                if (fd < 0) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
                args[i] = NULL;
            }
        } else if (strcmp(args[i], ">") == 0) {
            // Output redirection (truncate)
            if (i + 1 < arg_count) {
                fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                args[i] = NULL;
            }
        } else if (strcmp(args[i], ">>") == 0) {
            // Output redirection (append)
            if (i + 1 < arg_count) {
                fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                args[i] = NULL;
            }
        }
    }
}

// Handle command piping
void handle_piping(char *commands[], int cmd_count) {
    int pipes[MAX_COMMANDS-1][2];
    pid_t pids[MAX_COMMANDS];
    
    // Create all pipes
    for (int i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            return;
        }
    }
    
    // Execute all commands
    for (int i = 0; i < cmd_count; i++) {
        // Create a copy of the command for tokenization
        char cmd_copy[MAX_INPUT];
        strcpy(cmd_copy, commands[i]);
        
        // Parse command arguments
        char *args[MAX_ARGS];
        int arg_count = 0;
        
        char *token = strtok(cmd_copy, " \t");
        while (token != NULL && arg_count < MAX_ARGS - 1) {
            args[arg_count++] = token;
            token = strtok(NULL, " \t");
        }
        args[arg_count] = NULL;
        
        // Fork process
        pids[i] = fork();
        
        if (pids[i] < 0) {
            perror("fork");
            return;
        } else if (pids[i] == 0) {
            // Child process
            
            // Set up input from previous pipe
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            
            // Set up output to next pipe
            if (i < cmd_count - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            
            // Close all pipes in child
            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            // Handle redirections within the command
            handle_redirection(args, arg_count);
            
            // Execute command
            execvp(args[0], args);
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }
    
    // Parent process: close all pipes
    for (int i = 0; i < cmd_count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    
    // Wait for all children
    for (int i = 0; i < cmd_count; i++) {
        waitpid(pids[i], NULL, 0);
    }
}
// Execute multiple commands with ; or &&
void execute_multiple_commands(char *commands[], int cmd_count, char *separator) {
    int i;
    int last_status = 0;
    
    for (i = 0; i < cmd_count; i++) {
        // Skip if previous command failed with &&
        if (strcmp(separator, "&&") == 0 && last_status != 0) {
            continue;
        }
        
        // Parse command
        char *args[MAX_ARGS];
        int arg_count = 0;
        char *token = strtok(commands[i], " ");
        while (token != NULL && arg_count < MAX_ARGS - 1) {
            args[arg_count++] = token;
            token = strtok(NULL, " ");
        }
        args[arg_count] = NULL;
        
        // Execute command
        if (!is_builtin_command(args, arg_count)) {
            pid_t pid = fork();
            
            if (pid < 0) {
                perror("fork");
                last_status = 1;
                continue;
            } else if (pid == 0) {
                // Child process
                handle_redirection(args, arg_count);
                execvp(args[0], args);
                perror("execvp");
                exit(EXIT_FAILURE);
            } else {
                // Parent process
                int status;
                waitpid(pid, &status, 0);
                last_status = WEXITSTATUS(status);
            }
        }
    }
}

// Add command to history
void add_to_history(char *command) {
    if (history_count < MAX_COMMANDS) {
        strncpy(history[history_count], command, MAX_INPUT);
        history_count++;
    } else {
        // Shift history up
        for (int i = 0; i < MAX_COMMANDS - 1; i++) {
            strcpy(history[i], history[i + 1]);
        }
        strncpy(history[MAX_COMMANDS - 1], command, MAX_INPUT);
    }
}

// Print command history
void print_history() {
    for (int i = 0; i < history_count; i++) {
        printf("%d: %s\n", i + 1, history[i]);
    }
}
// Signal handler
void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("\nsh> ");
        fflush(stdout);
    }
}
