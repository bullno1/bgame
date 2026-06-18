#include <bgame/metrics.h>
#include <cute_time.h>

AUTOLIST_DEFINE(bgame__metrics)

void
bgame_set_gauge(const bgame_metric_decl_t* gauge, float value) {
	if (gauge->info->type == BGAME_METRIC_GAUGE) {
		atomic_store_explicit(&gauge->value->float_value, value, memory_order_relaxed);
	}
}

float
bgame_get_gauge(const bgame_metric_decl_t* gauge) {
	if (gauge->info->type == BGAME_METRIC_GAUGE) {
		return atomic_load_explicit(&gauge->value->float_value, memory_order_relaxed);
	} else {
		return 0.f;
	}
}

void
bgame_inc_counter(const bgame_metric_decl_t* counter, unsigned int value) {
	if (counter->info->type == BGAME_METRIC_COUNTER) {
		atomic_fetch_add_explicit(&counter->value->uint_value, 1, memory_order_relaxed);
	}
}

unsigned int
bgame_get_counter(const bgame_metric_decl_t* counter) {
	if (counter->info->type == BGAME_METRIC_COUNTER) {
		return atomic_load_explicit(&counter->value->uint_value, memory_order_relaxed);
	} else {
		return 0;
	}
}

double
bgame_get_rate(const bgame_metric_decl_t* counter, bgame_rate_sampler_t* sampler) {
	if (counter->info->type == BGAME_METRIC_COUNTER) {
		unsigned int value = bgame_get_counter(counter);

		if (sampler->timestamp_s == 0.0) {
			sampler->timestamp_s = CF_SECONDS;
			sampler->last_value = value;
			return 0.0;
		} else {
			double current_time = CF_SECONDS;
			unsigned int last_value = sampler->last_value;
			double rate = (double)(value - last_value) - (current_time - sampler->timestamp_s);
			sampler->last_value = value;
			sampler->timestamp_s = current_time;
			return rate;
		}
	} else {
		return 0.0;
	}
}
