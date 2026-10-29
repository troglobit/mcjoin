/* This is free and unencumbered software released into the public domain. */

#include <assert.h>
#include <stdint.h>
#include "pev.h"

static void unexpected_cb(int id, void *arg)
{
	(void)id;
	(void)arg;
	assert(0);
}

struct timer_ids {
	int timer;
	int periodic;
};

static void finish_cb(int id, void *arg)
{
	struct timer_ids *ids = arg;

	assert(pev_timer_get(id) == 1000);
	assert(pev_timer_get(ids->timer) == INT64_C(6000000000));
	assert(pev_timer_get(ids->periodic) == INT64_C(6000000000));
	assert(pev_timer_del(ids->timer) == 0);
	assert(pev_timer_del(ids->periodic) == 0);
	pev_exit(0);
}

int main(void)
{
	struct timer_ids ids;
	int timer, periodic;
	int64_t long_delay = INT64_C(6000) * 1000000;

	assert(pev_init() >= 0);
	timer = pev_timer_add(long_delay, 0, unexpected_cb, NULL);
	assert(timer >= 0);
	assert(pev_timer_get(timer) == long_delay);
	assert(pev_timer_set(timer, INT64_C(2147483648)) == 0);
	assert(pev_timer_get(timer) == INT64_C(2147483648));
	assert(pev_timer_set(timer, long_delay) == 0);

	periodic = pev_timer_add(0, long_delay, unexpected_cb, NULL);
	assert(periodic >= 0);
	assert(pev_timer_get(periodic) == long_delay);
	ids.timer = timer;
	ids.periodic = periodic;
	assert(pev_timer_add(1000, 0, finish_cb, &ids) >= 0);
	assert(pev_run() == 0);

	return 0;
}
