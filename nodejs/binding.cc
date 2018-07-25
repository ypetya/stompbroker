#include <node.h>
#include <v8.h>
#include <uv.h>

using namespace v8;
using namespace node; 
using namespace std;

#include "../lib/signal_action_handler.c"
#include "../lib/queue.c"
#include "../lib/thread_safe_queue.c"
#include "../lib/clone_str.c"
#include "../stomp/parse_args.c"
#include "../stomp/server/listen.c"
#include "../stomp/server/process.c"
#include "../stomp/server/accept.c"
#include "../stomp/stomp/stomp.c"
#include "../stomp/stomp/parser.c"
#include "../stomp/stomp/message.c"

#include <unistd.h>

namespace {

    static void asyncM(uv_work_t * arg) {
        ConfigStruct config = parseArgs();

        doListen(config.port, config.backlog);

    }

    void Method(const v8::FunctionCallbackInfo<v8::Value>& args) {
        //v8::Isolate* isolate = args.GetIsolate();

        uv_queue_work(uv_default_loop(), NULL, asyncM, NULL);

        //args.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, "world"));
    }

    inline void Initialize(v8::Local<v8::Object> binding) {
        v8::Isolate * const isolate = binding->GetIsolate();
        binding->Set(v8::String::NewFromUtf8(isolate, "method"),
                v8::FunctionTemplate::New(isolate, Method)->GetFunction());
    }

    NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

}