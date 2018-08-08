#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>


#define print(...) fprintf(stdout,__VA_ARGS__);
// define or alter log levels here
#define warn(...) fprintf(stderr,__VA_ARGS__);
#define info(...) print(__VA_ARGS__)

#ifdef DEBUG
#define debug(...) print(__VA_ARGS__)
#define trace(...) print(__VA_ARGS__)
#else
#define debug(...) // print(__VA_ARGS__)
#define trace(...) // print(__VA_ARGS__)
#endif

#endif