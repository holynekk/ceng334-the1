#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#include "parser.h"

#define P_BUF_SIZE 1024

void execute_subshell(char * subshell_string);

void execute_single_command(command cmd, bool isParalel) {
    int status;
    pid_t pid = fork();
    if (pid == 0) {
        execvp(cmd.args[0], cmd.args);
        exit(0);
    }
    if (!isParalel) {
        waitpid(pid, &status, 0);
    }
}

void execute_inner_pipeline(command * commands, int n, bool is_paralel) {
    pid_t pid;
    int ** pipes = (int **) malloc((n-1) * sizeof(int *)), status;

    for (int i = 0; i < n-1; i++) {
        pipes[i] = (int*) malloc(2 * sizeof(int));
        pipe(pipes[i]);
    }

    for (int i = 0; i < n; i++) {
        pid = fork();
        if (pid == 0) {
            if (i > 0) {
                dup2(pipes[i-1][0], 0);
            }
            if (i < n-1) {
                dup2(pipes[i][1], 1);
            }
            for (int k = 0; k < n-1; k++) {
                close(pipes[k][0]);
                close(pipes[k][1]);
            }
            execvp(commands[i].args[0], commands[i].args);
            exit(0);
        }
    }

    for (int k = 0; k < n-1; k++) {
        close(pipes[k][0]);
        close(pipes[k][1]);
    }

    if (!is_paralel) {
        for (int i = 0; i < n; i++) {
            wait(&status);
        }
    }

    free(pipes);
}

void execute_pipeline(single_input * pipeline_inputs, int n) {
    pid_t pid;
    int ** pipes = (int **) malloc((n-1) * sizeof(int *)), status;

    for (int i = 0; i < n-1; i++) {
        pipes[i] = (int*) malloc(2 * sizeof(int));
        pipe(pipes[i]);
    }
    for (int i = 0; i < n; i++) {
        pid = fork();
        if (pid == 0) {
            single_input input_val = pipeline_inputs[i];
            if (i > 0) {
                dup2(pipes[i-1][0], 0);
            }
            if (i < n-1) {
                dup2(pipes[i][1], 1);
            }
            for (int k = 0; k < n-1; k++) {
                close(pipes[k][0]);
                close(pipes[k][1]);
            }
            if (pipeline_inputs[i].type == INPUT_TYPE_COMMAND) {
                execvp(pipeline_inputs[i].data.cmd.args[0], pipeline_inputs[i].data.cmd.args);
            } else if (pipeline_inputs[i].type == INPUT_TYPE_SUBSHELL) {
                execute_subshell(pipeline_inputs[i].data.subshell);
            }
            
            exit(0);
        }
    }

    for (int k = 0; k < n-1; k++) {
        close(pipes[k][0]);
        close(pipes[k][1]);
    }

    for (int i = 0; i < n; i++) {
        wait(&status);
    }

    free(pipes);
}

void execute_sequential(single_input * pipeline_inputs, int n) {
    for (int i = 0; i < n; i++) {
        if (pipeline_inputs[i].type == INPUT_TYPE_COMMAND) {
            execute_single_command(pipeline_inputs[i].data.cmd, false);
        } else if (pipeline_inputs[i].type == INPUT_TYPE_PIPELINE) {
            execute_inner_pipeline(pipeline_inputs[i].data.pline.commands, pipeline_inputs[i].data.pline.num_commands, false);
        }
    }
}

void execute_paralel(single_input * pipeline_inputs, int n) {
    int wait_count = 0, status;

    for (int i = 0; i < n; i++) {
        if (pipeline_inputs[i].type == INPUT_TYPE_COMMAND) {
            wait_count++;
            execute_single_command(pipeline_inputs[i].data.cmd, true);
        } else if (pipeline_inputs[i].type == INPUT_TYPE_PIPELINE) {
            wait_count += pipeline_inputs[i].data.pline.num_commands;
            execute_inner_pipeline(pipeline_inputs[i].data.pline.commands, pipeline_inputs[i].data.pline.num_commands, true);
        }
    }
    for (int i = 0; i < wait_count; i++) {
        wait(&status);
    }
}

void execute_subshell(char * subshell_string) {
    int status, wait_count = 0;
    pid_t pid = fork();

    parsed_input *parsed_subshell = (parsed_input *) malloc(sizeof(parsed_input));
    parse_line(subshell_string, parsed_subshell);

    if (pid == 0) {
        if (parsed_subshell->separator == SEPARATOR_NONE && parsed_subshell->num_inputs == 1) {
            if (parsed_subshell->inputs[0].type == INPUT_TYPE_COMMAND) {
                /*
                Single command execution.
                */
                execute_single_command(parsed_subshell->inputs[0].data.cmd, false);
            }
        } else if (parsed_subshell->separator == SEPARATOR_PIPE) {
            /*
            Pipeline execution.
            */
            execute_pipeline(parsed_subshell->inputs, parsed_subshell->num_inputs);
        } else if (parsed_subshell->separator == SEPARATOR_SEQ) {
            /*
            Sequential execution.
            */
            execute_sequential(parsed_subshell->inputs, parsed_subshell->num_inputs);
        } else if (parsed_subshell->separator == SEPARATOR_PARA) {
            /*
            Paralel execution.
            */
            execute_paralel(parsed_subshell->inputs, parsed_subshell->num_inputs);

            // TODO: Implement repeater!

        //     int n = parsed_subshell->num_inputs;
        //     int ** pipes = (int **) malloc(n * sizeof(int *)), status;

        //     for (int i = 0; i < n; i++) {
        //         pipes[i] = (int*) malloc(2 * sizeof(int));
        //         pipe(pipes[i]);
        //     }
        //     for (int i = 0; i < n; i++) {
        //         if (parsed_subshell->inputs[i].type == INPUT_TYPE_COMMAND) {
        //             wait_count++;
        //         } else if (parsed_subshell->inputs[i].type == INPUT_TYPE_PIPELINE) {
        //             wait_count += parsed_subshell->inputs[i].data.pline.num_commands;
        //         }
        //         // printf("Subshell child: %d\n", getpid());
        //         pid_t child_pid = fork();
        //         if (child_pid == 0) {
        //             dup2(pipes[i][0], 0);
        //             if (parsed_subshell->inputs[i].type == INPUT_TYPE_COMMAND) {
        //                 // printf("Child got executed: %d\n", getpid());
        //                 execute_single_command(parsed_subshell->inputs[i].data.cmd, true);
        //             } else if (parsed_subshell->inputs[i].type == INPUT_TYPE_PIPELINE) {
        //                 execute_inner_pipeline(parsed_subshell->inputs[i].data.pline.commands, parsed_subshell->inputs[i].data.pline.num_commands, true);
        //             }
        //             // for (int k = 0; k < n; k++) {
        //             //     close(pipes[k][0]);
        //             //     close(pipes[k][1]);
        //             // }
        //             // printf("Child exiting: %d\n", getpid());
        //             exit(0);
        //         }
        //     }
        //     pid_t repeater_pid = fork();
        //     size_t contentSize = 1;
        //     char buffer[P_BUF_SIZE];
        //     char *content = malloc(P_BUF_SIZE * sizeof(char));
        //     content[0] = '\0';
        //     if (repeater_pid == 0) {
        //         while (fgets(buffer, P_BUF_SIZE, stdin)) {
        //             char *old = content;
        //             contentSize += strlen(buffer);
        //             content = realloc(content, contentSize);
        //             strcat(content, buffer);
        //         }
        //         for (int i = 0; i < n; i++) {
        //             write(pipes[i][1], content, contentSize);
        //         }
        //         // printf("Repeater exiting: %d\n", getpid());
        //         exit(0);
        //     }
        //     for (int k = 0; k < n; k++) {
        //         close(pipes[k][0]);
        //         close(pipes[k][1]);
        //     }
        }
        // for (int i = 0; i < 5; i++) {
        //     pid_t aaa = wait(&status);
        //     // printf("Recieved: %d\n", aaa);
        // }
        // // (ls -l /usr/lib | grep x) | ( tr /a-z/ /A-Z/ , echo done)
        // // (echo "a" , echo "b")
        // // printf("Subshell child exiting: %d\n", getpid());
        // exit(0);
    }
    // else {
        pid_t bumbum = wait(&status);
        // printf("Recieved: %d\n", bumbum);
    // }
}

int main(void) {
    char *line = malloc(INPUT_BUFFER_SIZE * sizeof(char));
    parsed_input *p_input = (parsed_input *) malloc(sizeof(parsed_input));
    printf("/> ");
    while (fgets(line, INPUT_BUFFER_SIZE, stdin)) {
        parse_line(line, p_input);
        if (p_input->separator == SEPARATOR_NONE && p_input->num_inputs == 1) {
            if (p_input->inputs[0].type == INPUT_TYPE_SUBSHELL) {
                /*
                Subshell execution.
                */
                execute_subshell(p_input->inputs[0].data.subshell);
            } else if (p_input->inputs[0].type == INPUT_TYPE_COMMAND) {
                if (strcmp(*(p_input->inputs[0].data.cmd.args), "quit") == 0) {
                    return 0;
                } else {
                    /*
                    Single command execution.
                    */
                    execute_single_command(p_input->inputs[0].data.cmd, false);
                }
            }
        } else if (p_input->separator == SEPARATOR_PIPE) {
            /*
            Pipeline execution.
            */
            execute_pipeline(p_input->inputs, p_input->num_inputs);
        } else if (p_input->separator == SEPARATOR_SEQ) {
            /*
            Sequential execution.
            */
            execute_sequential(p_input->inputs, p_input->num_inputs);
        } else if (p_input->separator == SEPARATOR_PARA) {
            /*
            Paralel execution.
            */
            execute_paralel(p_input->inputs, p_input->num_inputs);
        }
        printf("/> ");
    }
    free(line);
    free(p_input);
    return 0;
}
