#ifndef SYSINFO_COMPAT_H
#define SYSINFO_COMPAT_H

/**
 * Portable access to the processor count.
 *
 * Linux provides get_nprocs()/get_nprocs_conf() in <sys/sysinfo.h>.
 * Other POSIX systems (e.g. macOS) get an equivalent stub based on sysconf(),
 * so the portable modules and unit tests can be built there as well.
 */
#ifdef __linux__
#include <sys/sysinfo.h>
#else
#include <unistd.h>

static inline int get_nprocs(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int) n : 1;
}

static inline int get_nprocs_conf(void) {
    long n = sysconf(_SC_NPROCESSORS_CONF);
    return n > 0 ? (int) n : 1;
}
#endif

#endif /* SYSINFO_COMPAT_H */
