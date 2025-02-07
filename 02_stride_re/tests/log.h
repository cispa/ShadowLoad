#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>

#define _STR(x) #x

#define _DEFER(a, b) a(b)

#ifdef EVAL
    #define DEBUG(s, ...) ;
    #define INFO(s, ...) ;
#else
    #define DEBUG(s, ...) printf("D [" __FILE__ ":" _DEFER(_STR, __LINE__) "] " s __VA_OPT__(,) __VA_ARGS__)
    #define INFO(s, ...) printf("I [" __FILE__  ":" _DEFER(_STR, __LINE__) "] " s __VA_OPT__(,) __VA_ARGS__)
#endif /* EVAL */

#define WARN(s, ...) printf("! [" __FILE__ ":" _DEFER(_STR, __LINE__) "] " s __VA_OPT__(,) __VA_ARGS__)
#define ERROR(s, ...) fprintf(stderr, "X [" __FILE__ ":" _DEFER(_STR, __LINE__) "] " s __VA_OPT__(,) __VA_ARGS__)
#define FATAL(s, ...) fprintf(stderr, "# [" __FILE__ ":" _DEFER(_STR, __LINE__) "] " s __VA_OPT__(,) __VA_ARGS__); exit(-1)

// no need to print file for results
#define RESULT(s, ...) printf("R " s __VA_OPT__(,) __VA_ARGS__)

#endif /* LOG_H */
