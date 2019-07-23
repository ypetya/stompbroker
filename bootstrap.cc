#include <node.h>

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


using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::NewStringType;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

/**
 * # Responsibility
 * 
 * Fire-up standalone Server:
 * 1. Read config
 * 2. Start listening
 */
void run(const FunctionCallbackInfo<Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();

    if (args.Length() != 5) {
        // Throw an Error that is passed back to JavaScript
        isolate->ThrowException(Exception::TypeError(
                String::NewFromUtf8(isolate,
                "Wrong number of arguments. There must be exactly 5 numbers: "
                "processors, port, max_input_queue_size, max_stale_queue_size, ttl\n"
                "pass 0 for pick the default.",
                NewStringType::kNormal).ToLocalChecked()));
        return;
    }
    for (int i = 0; i < 5; i++) {
        if (!args[i]->IsNumber()) {
            // Throw an Error that is passed back to JavaScript
            isolate->ThrowException(Exception::TypeError(
                    String::NewFromUtf8(isolate,
                    "Arguments must be a type of number. There must be exactly 5 numbers: "
                    "processors, port, max_input_queue_size, max_stale_queue_size, ttl\n"
                    "pass 0 for pick the default.",
                    NewStringType::kNormal).ToLocalChecked()));
            return;
        }
    }

    print_system_info();

    stomp_app_config config = config_setup_by_args(args[0].As<Number>()->Value(),
            args[1].As<Number>()->Value(),
            args[2].As<Number>()->Value(),
            args[3].As<Number>()->Value(),
            args[4].As<Number>()->Value()
            );

    do_listen(config.port, config.backlog);
}

void runDefault(const FunctionCallbackInfo<Value>& args) {
    print_system_info();
    stomp_app_config config = config_parse_args(0, NULL);

    do_listen(config.port, config.backlog);
}

void Init(Local<Object> exports) {
    NODE_SET_METHOD(exports, "runDefault", runDefault);
    NODE_SET_METHOD(exports, "run", run);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Init)
        
