#include <napi.h>



//#define DEBUG 1
//#define DEBUG_OUTPUT

// pick-up all the implementations

#include "lib/emalloc.c"
#include "lib/general_list.c"
#include "lib/queue.c"
#include "lib/thread_safe_queue.c"
#include "lib/clone_str.c"
#include "lib/associative_array.c"

#include "print_system_info.c"
#include "parse_args.c"


#include "websocket/main.h"
#include "stomp/main.h"
#include "server/main.h"
/**
 * # Responsibility
 * 
 * Fire-up standalone Server:
 * 1. Read config
 * 2. Start listening
*/
Napi::Value runSync(const Napi::CallbackInfo& info) {
    print_system_info();
    stomp_app_config config = config_parse_args(0,NULL);
    
    do_listen(config.port, config.backlog);
    
    return Napi::Number::New(info.Env(), 1);
}


Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "runSync"), Napi::Function::New(env, runSync));
  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)