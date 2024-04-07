#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <stdbool.h>
#include <sys/wait.h>

#include "parser.h"

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
    int status;
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
        }
        exit(0);
    } else {
        wait(&status);
    }
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

// ls -l | tr /a-z/ /A-Z/
// cat input.txt | grep "log" | wc -c
// ls -l ; ls -al /dev
// ls -l | tr /a-z/ /A-Z/ ; echo "Done."
// cat input.txt | grep "log" | wc -c ; ls -al /dev
// echo "1" , echo "2"
// ls -l | tr /a-z/ /A-Z/ , echo "Done."
// cat input.txt | grep "log" | wc -c , ls -al /dev
// (echo "1." ; echo "2." ; echo "3.")
// (ls -l | tr /a-z/ /A-Z/ ; echo "Done.")
// (ls -l | tr /a-z/ /A-Z/ , echo "Done.")
// (ls -l | tr /a-z/ /A-Z/ , echo "Done.") | (cat ; echo "Hello"; cat input.txt) | cat | (wc -c , wc -l)
// (cat input.txt | grep "c") | (tr /a-z/ /A-Z/ ; ls -al /dev) | (cat | wc -l , cat , grep "A")
// (ls -l | grep a ) | (cat ; echo Hello ; ls -al)
// (ls -l /usr/bin | grep x) | (tr /a-c/ /A-C/; echo done)
// (ls -l /usr/bin | grep x) | tr /a-c/ /A-C/
// ls -al | tr /a-l/ /A-L/ | ( grep A , grep B , wc -l , wc -c )
// quit

// -- Tester --
// Testcase0:  ls -l                                                                  -- OK
// Testcase1:  ls -l ; ls -al                                                         -- OK
// Testcase2:  echo 'Hello'                                                           -- OK
// Testcase3:  ls -l | tr /a-z/ /A-Z/                                                 -- OK
// Testcase4:  ls -l , ls -al                                                         -- OK
// Testcase5:  ls -l ; echo 'Hello'                                                   -- OK
// Testcase6:  (ls -l ; echo 'Hello') | grep x | tr /a-g/ /A-G/                       -- OK
// Testcase7:  (ls -l /usr/bin | grep x) | ( tr /a-c/ /A-C/ , echo done)              -- FAILED
// Testcase8:  (ls -l /usr/bin | grep x) | ( tr /a-z/ /A-Z/ , echo done) | wc -l      -- FAILED
// Testcase9:  (ls -l /usr/bin | grep a ) | (cat ; echo Hello ; ls -al /usr/lib)      -- FAILED
// Testcase10: ls -al /usr/bin | tr /a-l/ /A-L/ | ( grep A , grep B )                 -- FAILED
// Testcase11: ls -al /usr/bin | tr /a-l/ /A-L/ | ( grep A , grep B , wc -l )         -- FAILED
// Testcase12: ls -al /usr/bin | tr /a-l/ /A-L/ | ( grep A , grep B , wc -l , wc -c ) -- FAILED
