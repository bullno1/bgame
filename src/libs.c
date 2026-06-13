#include <bgame/reloadable.h>
#include <bgame/allocator.h>
#include <blog.h>
#include <cute.h>

#define BLIB_REALLOC bgame_realloc
#define BLIB_IMPLEMENTATION
#include <bhash.h>
#include <barray.h>
#include <barena.h>

#undef BARRAY_REALLOC
#define BENT_LOG BLOG_DEBUG
#define BENT_ASSERT CF_ASSERT
#include <bent.h>

#if BGAME_RELOADABLE
#include <bresmon.h>
#endif

#define CLAY_IMPLEMENTATION
#include <clay.h>
