#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_INPUT_SIZE 1024
#define MAX_VARIABLES 64
#define MAX_HISTORY 20
#define MAX_JOBS 20

char* variables[MAX_VARIABLES]; // Array to store environment variables

struct CommandHistory {
    char* commands[MAX_HISTORY];
    int count;
    int current; // Index of the current command in history
};

struct Job {
    int job_id;  // Unique job identifier
    int pid;     // Process ID
    int status;  // 0 for running, 1 for stopped
};

void add_to_history(struct CommandHistory* history, const char* command) {
    if (history->count < MAX_HISTORY) {
        history->commands[history->count] = strdup(command);
        history->count++;
    } else {
        free(history->commands[0]);
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            history->commands[i] = history->commands[i + 1];
        }
        history->commands[MAX_HISTORY - 1] = strdup(command);
    }
}

int find_variable(char* name) {
    for (int i = 0; i < MAX_VARIABLES; i++) {
        if (variables[i] == NULL) {
            return -1; // Variable not found
        }
        if (strncmp(variables[i], name, strlen(name)) == 0) {
            return i; // Variable found
        }
    }
    return -1; // Variable not found
}
int find_empty_job_slot(struct Job* jobs) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid == 0) {
            return i; // Found an empty slot
        }
    }
    return -1; // No empty slots
}
void handle_signal(int signo) {
    // Handle Ctrl+C (SIGINT) signal
    if (signo == SIGINT) {
        // Send the signal to the foreground process group
        kill(0, SIGINT);
    }
}

void list_jobs(struct Job* jobs) {
    printf("Jobs:\n");
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid != 0) {
            printf("[%d] %d %s\n", jobs[i].job_id, jobs[i].pid, (jobs[i].status == 0) ? "Running" : "Stopped");
        }
    }
}

void display_command_history(struct CommandHistory* history) {
    printf("Command History:\n");
    for (int i = 0; i < history->count; i++) {
        printf("%d: %s\n", i + 1, history->commands[i]);
    }
}
void run_script(const char* script_filename) {
    char line[MAX_INPUT_SIZE];
    FILE* script_file = fopen(script_filename, "r");
    if (script_file == NULL) {
        perror(script_filename);
        return;
    }

    while (fgets(line, sizeof(line), script_file)) {
        // Execute each line from the script
        system(line);
    }

    fclose(script_file);
}
void expand_variables(char* command) {
    char* variable_name = strchr(command, '$');
    while (variable_name != NULL) {
        char* end = strpbrk(variable_name, " \t\n");
        if (end != NULL) {
            *end = '\0'; // Null-terminate the variable name
        }
        char* variable_value = getenv(variable_name + 1); // Skip the '$' character
        if (variable_value != NULL) {
            memmove(variable_name, variable_value, strlen(variable_value) + 1);
        }
        variable_name = strchr(variable_name + 1, '$'); // Find the next variable
    }
}
void execute_myuniquecmd() {
    printf("This is a unique custom command!\n");
    printf("Feel free to modify it to do something special for you.\n");
    printf("You can customize your shell as you like.\n");
}
void execute_command_with_redirection(char* command) {
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stdin = dup(STDIN_FILENO);

    // Check for input redirection ("<")
    char* input_redirect = strstr(command, "<");
    if (input_redirect) {
        *input_redirect = '\0'; // Null-terminate the command
        char* input_file = strtok(input_redirect + 1, " \t\n");
        int input_fd = open(input_file, O_RDONLY);
        if (input_fd == -1) {
            perror("Input redirection");
            return;
        }
        dup2(input_fd, STDIN_FILENO);
        close(input_fd);
    }

    // Check for output redirection (">")
    char* output_redirect = strstr(command, ">");
    if (output_redirect) {
        *output_redirect = '\0'; // Null-terminate the command
        char* output_file = strtok(output_redirect + 1, " \t\n");
        int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_fd == -1) {
            perror("Output redirection");
            return;
        }
        dup2(output_fd, STDOUT_FILENO);
        close(output_fd);
    }
        // Execute the command
    system(command);

    // Restore the original stdin and stdout
    dup2(saved_stdin, STDIN_FILENO);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdin);
    close(saved_stdout);
}
char* replace_string(const char* source, const char* target, const char* replacement) {
    char* buffer = (char*)malloc(MAX_INPUT_SIZE);
    if (buffer == NULL) {
        return NULL;
    }

    char* buffer_p = buffer;
    const char* source_p = source;

    while (*source_p) {
        if (strstr(source_p, target) == source_p) {
            strcpy(buffer_p, replacement);
            source_p += strlen(target);
            buffer_p += strlen(replacement);
        } else {
            *buffer_p++ = *source_p++;
        }
    }

    *buffer_p = '\0';

    return buffer;
}
void run_script_with_args(const char* script_filename, char* args[], int arg_count) {
    char line[MAX_INPUT_SIZE];
    FILE* script_file = fopen(script_filename, "r");
    if (script_file == NULL) {
        perror(script_filename);
        return;
    }

    while (fgets(line, sizeof(line), script_file)) {
        // Execute each line from the script
        // Replace $1, $2, etc., with the actual arguments
        for (int i = 1; i <= arg_count; i++) {
            char arg_var[4];  // Assumes a maximum of 3 digits for arguments (e.g., $999)
            snprintf(arg_var, sizeof(arg_var), "$%d", i);
            char* arg_value = args[i - 1];
            // Replace $i with the actual argument value
            char* replaced_line = replace_string(line, arg_var, arg_value);
            system(replaced_line);
            free(replaced_line);
        }
    }

    fclose(script_file);
}


int main() {
    char input[MAX_INPUT_SIZE];
    char* args[64];  // Maximum 64 arguments for a command
    int status;
    int pipefd[2];  // Pipe file descriptors
    int input_fd, output_fd;

    struct CommandHistory history;
    history.count = 0;
    history.current = 0;

    struct Job jobs[MAX_JOBS];
    for (int i = 0; i < MAX_JOBS; i++) {
        jobs[i].job_id = 0;
        jobs[i].pid = 0;
        jobs[i].status = 0;
    }

    int job_counter = 0; // Counter to assign unique job IDs

    // Register the signal handler for Ctrl+C (SIGINT)
    signal(SIGINT, handle_signal);

    while (1) {
        printf("JAY-CMD $ ");  // Display the shell prompt

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;  // Exit on Ctrl+D or EOF
        }

        // Call expand_variables to expand environment variables
        expand_variables(input);

        // ... (rest of the code)
        
        // Parse the input into arguments
        int arg_count = 0;
        char* token = strtok(input, " \t\n");
        while (token != NULL) {
            args[arg_count] = token;
            arg_count++;
            token = strtok(NULL, " \t\n");
        }
        args[arg_count] = NULL;

        if (arg_count > 0) {
            // Handle built-in commands
            if (strcmp(args[0], "var-set") == 0) {
                if (arg_count < 3) {
                    fprintf(stderr, "Usage: set <variable_name> <variable_value>\n");
                } else {
                    char* variable_name = args[1];
                    char* variable_value = args[2];
                    setenv(variable_name, variable_value, 1);
                    variables[find_variable(variable_name)] = variable_name;
                }
            } else if (strcmp(args[0], "export-it") == 0) {
    if (arg_count < 2) {
        fprintf(stderr, "Usage: export <variable_name>\n");
    } else {
        char* variable_name = args[1];
        char* variable_value = getenv(variable_name);
        if (variable_value != NULL) {
            setenv(variable_name, variable_value, 1);
            variables[find_variable(variable_name)] = variable_name;
        } else {
            fprintf(stderr, "Variable found: %s\n", variable_name);
        }
    }
            } else if (strcmp(args[0], "var-unset") == 0) {
                if (arg_count < 2) {
                    fprintf(stderr, "Usage: unset <variable_name>\n");
                } else {
                    char* variable_name = args[1];
                    unsetenv(variable_name);
                    int index = find_variable(variable_name);
                    if (index != -1) {
                        free(variables[index]);
                        variables[index] = NULL;
                    }
                } }else if (strcmp(args[0], "jay-newfolder") == 0) {
                    if (arg_count < 2) {
                        fprintf(stderr, "Usage: newfolder <directory_name>\n");
                    } else {
                        if (mkdir(args[1], 0755) == 0) {
                            printf("Directory created: %s\n", args[1]);
                        } else {
                            perror("mkdir");
                        }
                    }
                } else if (strcmp(args[0], "jay-shiftfolder") == 0) {
                    if (arg_count < 2) {
                        fprintf(stderr, "Usage: shiftfolder <directory>\n");
                    } else {
                        if (chdir(args[1]) == 0) {
                            printf("Changed directory to: %s\n", args[1]);
                        } else {
                            perror("cd");
                        }
                    }
                } else if (strcmp(args[0], "history") == 0) {
                        display_command_history(&history);
                   } else if (strcmp(args[0], "myuniquecmd") == 0) {
                        execute_myuniquecmd();
                    } else if (strcmp(args[0], "jay-runscript") == 0) {
                if (arg_count < 2) {
                    fprintf(stderr, "Usage: runscript <script_filename> [arg1 arg2 ...]\n");
                } else {
                    run_script_with_args(args[1], &args[2], arg_count - 2);
                }
            } else {
                    // Check if the command should run in the background
                int run_in_background = 0;
                if (strcmp(args[arg_count - 1], "&") == 0) {
                    run_in_background = 1;
                    args[arg_count - 1] = NULL;  // Remove the "&" from the arguments
                }

                // Check for pipes and redirection
                int pipe_count = 0;
                for (int i = 0; i < arg_count; i++) {
                    if (strcmp(args[i], "|") == 0) {
                        args[i] = NULL;  // Null-terminate before the pipe
                        pipe_count++;
                    } else if (strcmp(args[i], "<") == 0) {
                        if (i < arg_count - 1) {
                            input_fd = open(args[i + 1], O_RDONLY);
                            if (input_fd == -1) {
                                perror("Input file open error");
                                break;
                            }
                            dup2(input_fd, STDIN_FILENO);
                            close(input_fd);
                            args[i] = NULL;
                        } else {
                            fprintf(stderr, "No input file specified.\n");
                        }
                    } else if (strcmp(args[i], ">") == 0) {
                        if (i < arg_count - 1) {
                            output_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                            if (output_fd == -1) {
                                perror("Output file open error");
                                break;
                            }
                            dup2(output_fd, STDOUT_FILENO);
                            close(output_fd);
                            args[i] = NULL;
                        } else {
                            fprintf(stderr, "No output file specified.\n");
                        }
                    }
                }

                if (pipe_count > 0) {
                    int pipe_command = 0;

                    for (int i = 0; i <= pipe_count; i++) {
                        if (pipe(pipefd) == -1) {
                            perror("Pipe creation error");
                            exit(1);
                        }

                        pid_t pid = fork();

                        if (pid < 0) {
                            perror("Fork failed");
                            exit(1);
                        } else if (pid == 0) {
                            // Child process
                            if (i > 0) {
                                dup2(pipe_command, STDIN_FILENO);
                                close(pipe_command);
                            }
                            if (i < pipe_count) {
                                dup2(pipefd[1], STDOUT_FILENO);
                                close(pipefd[1]);
                            }

                            if (execvp(args[pipe_command], args + pipe_command) == -1) {
                                perror("Command execution error");
                                exit(1);
                            }
                        } else {
                            // Parent process
                            close(pipe_command);
                            close(pipefd[1]);
                            pipe_command += (i + 1);

                            if (run_in_background) {
                                int job_slot = find_empty_job_slot(jobs);
                                if (job_slot != -1) {
                                    job_counter++;
                                    jobs[job_slot].job_id = job_counter;
                                    jobs[job_slot].pid = pid;
                                    jobs[job_slot].status = 0;  // Job is running
                                } else {
                                    fprintf(stderr, "Too many background jobs.\n");
                                }
                            } else {
                                wait(&status);
                            }
                        }
                    }
                } else {
                    // No pipes, execute a single command
                    if (run_in_background) {
                        int job_slot = find_empty_job_slot(jobs);
                        if (job_slot != -1) {
                            job_counter++;
                            pid_t pid = fork();
                            if (pid == 0) {
                                setpgid(0, 0);
                                if (execvp(args[0], args) == -1) {
                                    perror("Command execution error");
                                    exit(1);
                                }
                            } else {
                                // Parent process
                                jobs[job_slot].job_id = job_counter;
                                jobs[job_slot].pid = pid;
                                tcsetpgrp(STDIN_FILENO, getpid());
                                jobs[job_slot].status = 0;  // Job is running
                            }
                        } else {
                            fprintf(stderr, "Too many background jobs.\n");
                        }
                    } else {
                        pid_t pid = fork();
                        if (pid == 0) {
                            if (execvp(args[0], args) == -1) {
                                perror("Command execution error");
                                exit(1);
                            }
                        } else {
                            // Parent process
                            waitpid(pid, &status, WUNTRACED);
                            if (WIFSTOPPED(status)) {
                                // Check if the process was stopped
                                int job_id_to_update = -1;
                                for (int i = 0; i < MAX_JOBS; i++) {
                                    if (jobs[i].pid == pid) {
                                        job_id_to_update = jobs[i].job_id;
                                        jobs[i].status = 1;  // Job is stopped
                                    }
                                }
                                if (job_id_to_update != -1) {
                                    printf("[%d] Stopped %s\n", job_id_to_update, args[0]);
                                }
                            }
                            tcsetpgrp(STDIN_FILENO, getpid());
                        }
                    }
                }
            }
        }

}
    // Clean up: Send SIGTERM to background jobs
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid != 0) {
            kill(jobs[i].pid, SIGTERM);
        }
    }

    printf("Shell exited.\n");
    return 0;

}
