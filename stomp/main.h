

#ifndef STOMP_MAIN_H
#define STOMP_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "data_wrappers/session.c"
#include "data_wrappers/pub_sub.c"
#include "messages/message.c"
#include "messages/create_diagnostic_message.c"
#include "messages/distribute_messages.c"
#include "parser.c"
#include "stomp.c"


#ifdef __cplusplus
}
#endif

#endif /* STOMP_MAIN_H */

