#include "stomp.h"
#include "../logger.h"

void doStomp(char *input, int inputLen) {
    input[inputLen]='\0';
    print("Stomp received: %s",input);
}