#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/wait.h>

#include "parser.h"

#define BUF_SIZE 128

int main(void) {
    char *line = malloc(BUF_SIZE * sizeof(char));
    int status;
    pid_t pid;

    parsed_input *p_input = (parsed_input *) malloc(sizeof(parsed_input));
    printf("/> ");
    while (fgets(line, BUF_SIZE, stdin)) {
        parse_line(line, p_input);
        // pretty_print(p_input);
        if (p_input->separator == SEPARATOR_NONE && p_input->num_inputs == 1) {
            if (strcmp(*(p_input->inputs[0].data.cmd.args), "quit") == 0) {
                return 0;
            } else {
                /*
                Single command execution.
                */
                pid = fork();
                if (pid != 0) {
                    waitpid(pid, &status, 0);
                } else {
                    execvp(p_input->inputs[0].data.cmd.args[0], p_input->inputs[0].data.cmd.args);
                    exit(0);
                }
            }
        } else if (p_input->separator == SEPARATOR_PIPE) {
            /*
            Pipeline execution.
            */
            int n = p_input->num_inputs;
            int ** pipes = (int **) malloc((n-1) * sizeof(int *));
            for (int i = 0; i < n-1; i++) {
                pipes[i] = (int*) malloc(2 * sizeof(int));
                pipe(pipes[i]);
            }
            for (int i = 0; i < n; i++) {
                pid = fork();
                if (pid == 0) {
                    single_input input_val = p_input->inputs[i];
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
                    execvp(p_input->inputs[i].data.cmd.args[0], p_input->inputs[i].data.cmd.args);
                    exit(0);
                }
            }
            pid_t pidddd;
            for (int k = 0; k < n-1; k++) {
                close(pipes[k][0]);
                close(pipes[k][1]);
            }
            for (int i = 0; i < n; i++) {
                pidddd = wait(&status);
            }
            free(pipes);
        }
        printf("/> ");
    }
    free(line);
    free(p_input);

    return 0;
}

// ls -l | tr /a-z/ /A-Z/
// cat input.txt | grep "log" | wc -c
// ls -l | tr /a-z/ /A-Z/ ; echo "Done."
// cat input.txt | grep "log" | wc -c ; ls -al /dev
// ls -l | tr /a-z/ /A-Z/ , echo "Done."
// cat input.txt | grep "log" | wc -c , ls -al /dev
// (ls -l | tr /a-z/ /A-Z/ ; echo "Done.") -- There is a problem here!!!!!!
// (ls -l | tr /a-z/ /A-Z/ , echo "Done.") -- There is a problem here!!!!!!
// (ls -l | tr /a-z/ /A-Z/ , echo "Done.") | (cat ; echo "Hello"; cat input.txt) | cat | (wc -c , wc -l)
// (cat input.txt | grep "c") | (tr /a-z/ /A-Z/ ; ls -al /dev) | (cat | wc -l , cat , grep "A")
// quit
