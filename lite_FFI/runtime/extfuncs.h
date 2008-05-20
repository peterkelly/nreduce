#ifndef EXTFUNCS_H_
#define EXTFUNCS_H_

#endif /*EXTFUNCS_H_*/

#include "src/nreduce.h"
#include "compiler/source.h"
#include "compiler/util.h"
#include "runtime/builtins.h"
#include "runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#include "runtime/rngs.h"
#include "runtime/rvgs.h"
#include <zzip/zzip.h>
#include <wand/MagickWand.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

/*** head files end ***/

#define NUM_EXTFUNCS     14


extern const extfunc extfunc_info[NUM_EXTFUNCS];
