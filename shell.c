#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define INPUT_BUFFER 1024
#define ARG_LIMIT 64
#define COMMAND_LIMIT 16

char history[COMMAND_LIMIT][INPUT_BUFFER];
int history_len = 0;

void run_command(char *args[], int arg_counter);
void redir_handler(char *args[], int arg_counter);
void pipe_handler(char *commands[], int command_counter);
void multi_command_handler(char *commands[], int command_counter, char *separator);
void history_save(char *command);
void display_history();
void signal_handler(int sig);

int check_builtin(char *args[], int arg_counter)
{
    if (strcmp(args[0], "cd") == 0)
    {
        if (arg_counter < 2)
        {
            chdir(getenv("HOME"));
        }
        else
        {
            if (chdir(args[1]) != 0)
            {
                perror("cd");
            }
        }
        return 1;
    }
    else if (strcmp(args[0], "history") == 0)
    {
        display_history();
        return 1;
    }
    else if (strcmp(args[0], "exit") == 0)
    {
        exit(0);
    }
    return 0;
}

int main()
{
    char input[INPUT_BUFFER];
    char *args[ARG_LIMIT];

    signal(SIGINT, signal_handler);

    while (1)
    {
        printf("sh> ");
        fflush(stdout);

        if (!fgets(input, INPUT_BUFFER, stdin))
        {
            break; 
        }

        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0)
        {
            continue;
        }

        history_save(input);

        char temp_input[INPUT_BUFFER];
        strcpy(temp_input, input);

        if (strchr(input, '|') != NULL)
        {
            char *pipe_cmd[COMMAND_LIMIT];
            int pipe_nums = 0;

            char *pipe_token = strtok(temp_input, "|");
            while (pipe_token != NULL && pipe_nums < COMMAND_LIMIT)
            {
                while (*pipe_token == ' ')
                    pipe_token++;

                pipe_cmd[pipe_nums++] = pipe_token;
                pipe_token = strtok(NULL, "|");
            }

            pipe_handler(pipe_cmd, pipe_nums);
            continue;
        }

        if (strstr(input, ";") || strstr(input, "&&"))
        {
            char *commands[COMMAND_LIMIT];
            int command_counter = 0;

            strcpy(temp_input, input);

            char *separator = strstr(input, "&&") ? "&&" : ";";

            char *cmd_token = strtok(temp_input, separator);
            while (cmd_token != NULL && command_counter < COMMAND_LIMIT)
            {
                commands[command_counter++] = cmd_token;
                cmd_token = strtok(NULL, separator);
            }

            multi_command_handler(commands, command_counter, separator);
            continue; 
        }

        int arg_counter = 0;
        char *token = strtok(input, " \t");
        while (token != NULL && arg_counter < ARG_LIMIT - 1)
        {
            args[arg_counter++] = token;
            token = strtok(NULL, " \t");
        }
        args[arg_counter] = NULL;

        if (!check_builtin(args, arg_counter))
        {
            run_command(args, arg_counter);
        }
    }

    return 0;
}
void run_command(char *args[], int arg_counter)
{
    pid_t child = fork();

    if (child < 0)
    {
        perror("fork");
        return;
    }
    else if (child == 0)
    {

        redir_handler(args, arg_counter);
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
    else
    {

        waitpid(child, NULL, 0);
    }
}

void redir_handler(char *args[], int arg_counter)
{
    int i;
    int file;

    for (i = 0; i < arg_counter; i++)
    {
        if (strcmp(args[i], "<") == 0)
        {
            if (i + 1 < arg_counter)
            {
                file = open(args[i + 1], O_RDONLY);
                if (file < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(file, STDIN_FILENO);
                close(file);
                args[i] = NULL;
            }
        }
        else if (strcmp(args[i], ">") == 0)
        {
            if (i + 1 < arg_counter)
            {
                file = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (file < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(file, STDOUT_FILENO);
                close(file);
                args[i] = NULL;
            }
        }
        else if (strcmp(args[i], ">>") == 0)
        {
            if (i + 1 < arg_counter)
            {
                file = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (file < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(file, STDOUT_FILENO);
                close(file);
                args[i] = NULL;
            }
        }
    }
}

void pipe_handler(char *commands[], int command_counter)
{
    int pipes[COMMAND_LIMIT - 1][2];
    pid_t child_pids[COMMAND_LIMIT];

    for (int i = 0; i < command_counter - 1; i++)
    {
        if (pipe(pipes[i]) < 0)
        {
            perror("pipe");
            return;
        }
    }

    for (int i = 0; i < command_counter; i++)
    {

        char cmd_copy[INPUT_BUFFER];
        strcpy(cmd_copy, commands[i]);

        char *args[ARG_LIMIT];
        int arg_counter = 0;

        char *token = strtok(cmd_copy, " \t");
        while (token != NULL && arg_counter < ARG_LIMIT - 1)
        {
            args[arg_counter++] = token;
            token = strtok(NULL, " \t");
        }
        args[arg_counter] = NULL;

        child_pids[i] = fork();

        if (child_pids[i] < 0)
        {
            perror("fork");
            return;
        }
        else if (child_pids[i] == 0)
        {
            if (i > 0)
            {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }

            if (i < command_counter - 1)
            {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            for (int j = 0; j < command_counter - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            redir_handler(args, arg_counter);
            execvp(args[0], args);
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < command_counter - 1; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    for (int i = 0; i < command_counter; i++)
    {
        waitpid(child_pids[i], NULL, 0);
    }
}
void multi_command_handler(char *commands[], int command_counter, char *separator)
{
    int i;
    int last_status = 0;

    for (i = 0; i < command_counter; i++)
    {
        if (strcmp(separator, "&&") == 0 && last_status != 0)
        {
            continue;
        }

        char *args[ARG_LIMIT];
        int arg_counter = 0;
        char *token = strtok(commands[i], " ");
        while (token != NULL && arg_counter < ARG_LIMIT - 1)
        {
            args[arg_counter++] = token;
            token = strtok(NULL, " ");
        }
        args[arg_counter] = NULL;

        if (!check_builtin(args, arg_counter))
        {
            pid_t child = fork();

            if (child < 0)
            {
                perror("fork");
                last_status = 1;
                continue;
            }
            else if (child == 0)
            {
                redir_handler(args, arg_counter);
                execvp(args[0], args);
                perror("execvp");
                exit(EXIT_FAILURE);
            }
            else
            {
                int status;
                waitpid(child, &status, 0);
                last_status = WEXITSTATUS(status);
            }
        }
    }
}

void history_save(char *command)
{
    if (history_len < COMMAND_LIMIT)
    {
        strncpy(history[history_len], command, INPUT_BUFFER);
        history_len++;
    }
    else
    {
        for (int i = 0; i < COMMAND_LIMIT - 1; i++)
        {
            strcpy(history[i], history[i + 1]);
        }
        strncpy(history[COMMAND_LIMIT - 1], command, INPUT_BUFFER);
    }
}

void display_history()
{
    for (int i = 0; i < history_len; i++)
    {
        printf("%d: %s\n", i + 1, history[i]);
    }
}

void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        printf("\nsh> ");
        fflush(stdout);
    }
}

