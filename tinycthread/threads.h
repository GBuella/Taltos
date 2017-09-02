// The stdnoreturn.h header conflicts with Windows headers,
// which are included by tinycthread.h when building on Windows.
#ifdef noreturn
#undef noreturn
#include "tinycthread.h"
#define noreturn _Noreturn
#else
#include "tinycthread.h"
#endif
