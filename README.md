# Custom UNIX Shell in C

A custom implementation of a basic UNIX shell written in C, developed as part of an Operating Systems course project. This shell supports essential command-line functionalities including command execution, piping, redirection, signal handling, and command history.

---

## Features

This shell supports:

- **Command Prompt** – Displays a prompt (`sh>`) and reads user input.
- **Basic Command Execution** – Supports standard Linux commands using `fork()` and `execvp()`.
- **Input & Output Redirection**:
  - `<` for input redirection
  - `>` for output overwrite
  - `>>` for output append
- **Command Piping** – Execute chained commands using `|`, supporting any number of piped commands.
- **Multiple Commands**:
  - `;` to execute commands sequentially, regardless of success
  - `&&` to execute the next command only if the previous one succeeds
- **Command History** – Stores up to 100 previously executed commands in memory.
- **Signal Handling** – Handles `CTRL+C` gracefully:
  - Terminates only the running child process
  - Keeps the shell alive and ready for more commands

---

## How It Works

### 1. Main Loop
- Displays the prompt and reads user input using `fgets`.
- Ignores empty inputs and passes valid commands to the parser.

### 2. Command Parsing
- Splits input using:
  - `&&` for conditional execution
  - `;` for sequential execution
  - `|` for piping
- Tokenizes commands using `strtok()` and handles white spaces.

### 3. Execution
- Commands without pipes are executed via `fork()` and `execvp()`.
- Redirection (`<`, `>`, `>>`) is managed with `dup2()` and `open()`.
- Piped commands are executed with multiple forks and `pipe()` calls.

### 4. Signal Handling
- Sets up a signal handler using `signal()` or `sigaction()` for `SIGINT`.
- If a child process is running, `CTRL+C` terminates the child, not the shell.

---

## Example Usage

```bash
sh> pwd
/home/username/projects/myshell

sh> ls -l > files.txt

sh> cat < files.txt | grep .c >> cfiles.txt

sh> gcc main.c -o shell && ./shell

sh> cd /tmp; ls -a

sh> echo Hello && echo World


