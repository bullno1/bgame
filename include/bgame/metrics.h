#ifndef BGAME_METRICS_H
#define BGAME_METRICS_H

#include <stdatomic.h>
#include <bmacro.h>
#include <autolist.h>
#include "reloadable.h"

#define BGAME_DECLARE_METRIC(NAME) \
	extern bgame_metric_decl_t NAME;

#define BGAME_DEFINE_METRIC(NAME) \
	static bgame_metric_value_t BCONCAT(NAME, _value); \
	BGAME_PERSIST_VAR(BCONCAT(NAME, _value)) \
	extern bgame_metric_info_t BCONCAT(NAME, _info); \
	bgame_metric_decl_t NAME = { \
		.name = BSTRINGIFY(NAME), \
		.value = &BCONCAT(NAME, _value), \
		.info = &BCONCAT(NAME, _info), \
	}; \
	AUTOLIST_ADD_ENTRY(bgame__metrics, NAME, NAME) \
	bgame_metric_info_t BCONCAT(NAME, _info)

#define BGAME_FOREACH_METRIC(ITR) \
	AUTOLIST_FOREACH(bgame__metric_itr, bgame__metrics) \
		for ( \
			bgame_metric_decl_t* ITR = bgame__metric_itr->value_addr; \
			ITR != NULL; \
			ITR = NULL \
		)

typedef enum {
	BGAME_METRIC_COUNTER,
	BGAME_METRIC_GAUGE,
} bgame_metric_type_t;

typedef union {
	atomic_uint uint_value;
	_Atomic float float_value;
} bgame_metric_value_t;

typedef struct {
	bgame_metric_type_t type;
} bgame_metric_info_t;

typedef struct {
	const char* name;
	const bgame_metric_info_t* info;
	bgame_metric_value_t* value;
} bgame_metric_decl_t;

typedef struct {
	double timestamp_s;
	unsigned int last_value;
} bgame_rate_sampler_t;

void
bgame_set_gauge(const bgame_metric_decl_t* gauge, float value);

float
bgame_get_gauge(const bgame_metric_decl_t* gauge);

void
bgame_inc_counter(const bgame_metric_decl_t* counter, unsigned int value);

unsigned int
bgame_get_counter(const bgame_metric_decl_t* counter);

double
bgame_get_rate(const bgame_metric_decl_t* counter, bgame_rate_sampler_t* sampler);

AUTOLIST_DECLARE(bgame__metrics)

#endif
