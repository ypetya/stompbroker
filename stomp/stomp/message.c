
#include "message.h"
#include "../../lib/emalloc.h"
#include "../../lib/clone_str.h"

message * message_create(int fd, char * str, int len) {
    message * new_m = emalloc(sizeof(message));
    
    new_m->content = clone_str_len(str,len);
    new_m->fd = fd;
    
    return new_m;
}

void message_destroy(message * m){
    free(m->content);
    free(m);
}