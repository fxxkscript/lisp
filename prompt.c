#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

int main(int argc, char** argv) {
    puts("Lispy version 0.0.0.1");
    puts("Press Ctrl+c to Exit.\n");

    // never ending loop
    while (1) {
        /* Output for prompt */
        char *input = readline("lispy>");

        /* add input to history */
        add_history(input);

        /* echo back to user */
        printf("No you are a %s\n", input);

        /* free retrived input */
        free(input);
    }

    return 0;
}
