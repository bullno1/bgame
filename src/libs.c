#if defined(__linux__) && !defined(_GNU_SOURCE)
#	define _GNU_SOURCE 1
#endif

#include <bgame/reloadable.h>
#include <bgame/allocator.h>
#include <blog.h>
#include <cute.h>

#define BLIB_REALLOC bgame_realloc
#define BLIB_IMPLEMENTATION
#include <bhash.h>
#include <barray.h>
#include <bhandle.h>
#include <barena.h>
#include <bsfn.h>

#undef BHANDLE_REALLOC

#undef BARRAY_REALLOC
#define BENT_LOG BLOG_DEBUG
#define BENT_ASSERT CF_ASSERT
#include <bent.h>

#if BGAME_RELOADABLE
#include <bresmon.h>
#endif

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wextra-semi"
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4244 4305)
#endif

#define CLAY_IMPLEMENTATION
#include <clay.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
