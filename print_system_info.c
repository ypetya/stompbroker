
#include <sys/sysinfo.h>
#include <sys/resource.h>
#include "logger.h"

void print_system_info(){
    info("system: Has %d processors configured and %d processors available.\n",
         get_nprocs_conf(), get_nprocs());
    
    struct rlimit r={0};
    
    getrlimit(RLIMIT_NOFILE, &r);

    info("system: Maximum file descriptors (ulimit): %d.\n", r.rlim_cur);
}
