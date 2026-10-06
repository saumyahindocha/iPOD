#pragma once
typedef int mutex_t;
static inline void mutex_init(mutex_t *m) { (void)m; }
static inline void mutex_enter_blocking(mutex_t *m) { (void)m; }
static inline void mutex_exit(mutex_t *m) { (void)m; }
