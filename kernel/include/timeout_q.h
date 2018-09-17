#ifndef ZEPHYR_TIMEOUT_Q_H__
#define ZEPHYR_TIMEOUT_Q_H__

// FIXME: incestuous include, kernel.h defines struct _timeout, why?
#include <kernel.h>

#include <system_timer.h>

void _add_thread_timeout(struct k_thread *th, _wait_q_t *wq, s32_t ticks);

int _abort_thread_timeout(struct k_thread *th);

void _init_thread_timeout(struct _thread_base *th);

void _add_timeout(struct k_thread *th, struct _timeout *to,
                  _wait_q_t *wait_q, s32_t ticks);

int _abort_timeout(struct _timeout *to);

void _init_timeout(struct _timeout *to, _timeout_func_t fn);

s32_t _get_next_timeout_expiry(void);

#endif /* ZEPHYR_TIMEOUT_Q_H__ */
