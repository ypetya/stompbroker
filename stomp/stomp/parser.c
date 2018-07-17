#include "parser.h"
#include <stdio.h>
#include "../../lib/clone_str.h"

int parse_command(message* message);
char * parse_headers(associative_array * aa, char* str);
char * next_row(char * text);

parsed_message* parse_message(message* message) {
    size_t pm_size = sizeof (parsed_message);
    parsed_message* pm = emalloc(pm_size);

    pm->command = parse_command(message);

    if (pm->command > 0) {
        pm->headers = emalloc(sizeof (associative_array));

        char * next_line = next_row(message->content);
        next_line = parse_headers(pm->headers, next_line);
        next_line = next_row(next_line);
        if (next_line != NULL) pm->message_body = clone_str(next_line);
    }

    return pm;
}

void free_parsed_message(parsed_message* pm) {
    aa_free(pm->headers);
    free(pm->message_body);
    free(pm);
}

char * next_row(char * text) {
    if (text == '\0') return NULL;
    char*ln = strchr(text, '\n');
    if (ln == NULL) return NULL;
    if (*ln == '\r') ln++;
    if (ln == NULL) return NULL;
    return ln + 1;
}

char * line_end(char * text) {
    char *ln = strchr(text, '\n');
    char *cr = strchr(text, '\r');
    return ln == 0 ? (cr == 0 ? 0 : cr) :
            (cr == 0 ? ln : (ln - cr > 0 ? cr : ln));
}

char * parse_headers(associative_array * aa, char* str) {
    if (str == NULL) return NULL;

    char * le = line_end(str);
    char * nr_ptr = next_row(str);
    char * separator = strchr(str, ':');
    // ERR or end
    if (separator == NULL || le == NULL || (le - separator < 0)) return NULL;

    *separator = *le = '\0';
    aa_put(aa, str, separator + 1);
    *separator = ':';
    *le = '\n';

    return parse_headers(aa, nr_ptr);
}

#ifndef FRM_IS
#define FRM_IS(type) (memcmp(message->content, type, strlen(type))==0)
#endif

int parse_command(message* message) {
    if (FRM_IS(FRM_CONNECT) || FRM_IS(FRM_STOMP)) return FRM_CONNECT_ID;
    if (FRM_IS(FRM_DISCONNECT)) return FRM_DISCONNECT_ID;
    return -1;
}