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
    parsed_input *p_input = (parsed_input *) malloc(sizeof(parsed_input));
    printf("/> ");

    while (fgets(line, BUF_SIZE, stdin)) {
        parse_line(line, p_input);
        pretty_print(p_input);

        if (p_input->separator == 0 && p_input->num_inputs == 1 && strcmp(*(p_input->inputs[0].data.cmd.args), "quit") == 0) {
            return 0;
        }
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
// (ls -l | tr /a-z/ /A-Z/ ; echo "Done.")
// (ls -l | tr /a-z/ /A-Z/ , echo "Done.")
// (ls -l | tr /a-z/ /A-Z/ , echo "Done.") | (cat ; echo "Hello"; cat input.txt) | cat | (wc -c , wc -l)
// (cat input.txt | grep "c") | (tr /a-z/ /A-Z/ ; ls -al /dev) | (cat | wc -l , cat , grep "A")
// quit
