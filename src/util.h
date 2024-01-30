#ifndef tango_util_h
#define tango_util_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION

#define DEBUG_STRESS_GARBAGE_COLLECTION
#define DEBUG_LOG_GARBAGE_COLLECTION
#define UINT8_COUNT (UINT8_MAX + 1)


#undef DEBUG_STRESS_GARBAGE_COLLECTION
#undef DEBUG_LOG_GARBAGE_COLLECTION
#undef DEBUG_PRINT_CODE

#endif