#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <stdbool.h>
#include <sys/wait.h>

#include "parser.h"

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
            execvp(pipeline_inputs[i].data.cmd.args[0], pipeline_inputs[i].data.cmd.args);
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

int main(void) {
    char *line = malloc(INPUT_BUFFER_SIZE * sizeof(char));

    parsed_input *p_input = (parsed_input *) malloc(sizeof(parsed_input));
    printf("/> ");
    while (fgets(line, INPUT_BUFFER_SIZE, stdin)) {
        parse_line(line, p_input);
        // printf("Seperator: %d\n", p_input->separator);
        // printf("Num of inputs: %d\n", p_input->num_inputs);
        // pretty_print(p_input);
        if (p_input->separator == SEPARATOR_NONE && p_input->num_inputs == 1) {
            if (strcmp(*(p_input->inputs[0].data.cmd.args), "quit") == 0) {
                return 0;
            } else {
                /*
                Single command execution.
                */
                execute_single_command(p_input->inputs[0].data.cmd, false);
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

// ls -l | tr /a-z/ /A-Z/
// cat input.txt | grep "log" | wc -c
// ls -l ; ls -al /dev
// ls -l | tr /a-z/ /A-Z/ ; echo "Done."
// cat input.txt | grep "log" | wc -c ; ls -al /dev
// echo "1" , echo "2"
// ls -l | tr /a-z/ /A-Z/ , echo "Done."
// cat input.txt | grep "log" | wc -c , ls -al /dev
// (ls -l | tr /a-z/ /A-Z/ ; echo "Done.") -- There is a problem here!!!!!!
// (ls -l | tr /a-z/ /A-Z/ , echo "Done.") -- There is a problem here!!!!!!
// (ls -l | tr /a-z/ /A-Z/ , echo "Done.") | (cat ; echo "Hello"; cat input.txt) | cat | (wc -c , wc -l)
// (cat input.txt | grep "c") | (tr /a-z/ /A-Z/ ; ls -al /dev) | (cat | wc -l , cat , grep "A")
// quit
