#ifndef STM32H7_FORMAT_MACROS_H_
#define STM32H7_FORMAT_MACROS_H_

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <cinttypes>

#ifndef PRIu64
#define PRIu64 "llu"
#endif
#ifndef PRId64
#define PRId64 "lld"
#endif
#ifndef PRIi64
#define PRIi64 "lli"
#endif
#ifndef SCNu8
#define SCNu8 "hhu"
#endif

#endif
