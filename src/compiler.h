#ifndef tango_compiler_h
#define tango_compiler_h

#include "object.h"
#include "virtualmachine.h"

ObjectFunction* compile(const char* input);
void compilerCollectGarbage();

#endif