#include "parser.h"
#include <stdio.h>
#include "../lib/clone_str.h"

int parse_command(message_with_timestamp* message);
char * parse_headers(parsed_message* pm, associative_array * aa, char* str);
char * next_row(char * text);

parsed_message* parse_message(message_with_timestamp* message) {
    size_t pm_size = sizeof (parsed_message);
    parsed_message* pm = emalloc(pm_size);

    pm->command = parse_command(message);

    if (pm->command > 0) {
        pm->headers = emalloc(sizeof (associative_array));

        char * next_line = next_row(message->content);
        next_line = parse_headers(pm, pm->headers, next_line);

        if (next_line != NULL) {
            pm->message_body = clone_str(next_line);
        }
    }

    return pm;
}

void free_parsed_message(parsed_message* pm) {
    aa_free(pm->headers);
    free(pm->receipt_id);
    free(pm->message_body);
    free(pm->topic);
    free(pm->id);
    free(pm);
}

char * next_row(char * text) {
    if (text == NULL || *text == '\0') return NULL;
    char* ln = strchr(text, '\n');
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

/**
 * Parses a line of message as a header
 * 
 * @param pm : output : set special attributes of parsed message: receipt
 * @param aa : associative array of headers to append the line to recursively
 * @param str : the input message
 * @return 
 */
char * parse_headers(parsed_message *pm, associative_array * aa, char* str) {
    if (str == NULL) return NULL;

    char * le = line_end(str);
    char * nr_ptr = next_row(str);

    char * separator = strchr(str, ':');
    if (separator == NULL && nr_ptr != NULL) return nr_ptr;

    if (le > separator) {

        *separator = *le = '\0';
        aa_put(aa, str, separator + 1);
        if (strcmp(str, "receipt") == 0) {
            if (pm->receipt_id != NULL) free(pm->receipt_id);
            pm->receipt_id = clone_str(separator + 1);
            //debug(" * Header receipt: %s\n", pm->receipt_id);
        } else if (strcmp(str, "id") == 0) {
            if (pm->id != NULL) free(pm->id);
            pm->id = clone_str(separator + 1);
            //debug(" * Header id: %d\n", pm->id);
        } else if (strcmp(str, "destination") == 0) {
            if (pm->topic != NULL) free(pm->topic);
            pm->topic = clone_str(separator + 1);
            //debug(" * Header destination: %s\n", pm->topic);
        }
        *separator = ':';
        *le = '\n';

        return parse_headers(pm, aa, nr_ptr);
    } else {
        return nr_ptr;
    }
}

#ifndef FRM_IS
#define FRM_IS(type) (memcmp(message->content, type, strlen(type))==0)
#endif

int parse_command(message_with_timestamp* message) {
    if (FRM_IS(FRM_CONNECT) || FRM_IS(FRM_STOMP)) return FRM_CONNECT_ID;
    if (FRM_IS(FRM_DISCONNECT)) return FRM_DISCONNECT_ID;
    if (FRM_IS(FRM_SUBSCRIBE)) return FRM_SUBSCRIBE_ID;
    if (FRM_IS(FRM_UNSUBSCRIBE)) return FRM_UNSUBSCRIBE_ID;
    if (FRM_IS(FRM_SEND)) return FRM_SEND_ID;
    if (FRM_IS(FRM_DIAGNOSTIC)) return FRM_DIAGNOSTIC_ID;

    return -1;
}