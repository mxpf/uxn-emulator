#ifndef ACCOUNT_H
#define ACCOUNT_H
#include "constellation_runner.h"
/* Private experimental application harness, not a shared host service.
 * Zero-initialize, start once, and free. No live object may be moved. */
enum { ACCOUNT_EVENTS = 2048, ACCOUNT_OUTPUT = 8192, ACCOUNT_INPUTS = 64 };
typedef struct {
  ConstellationRunner runner;
  RoutedEvent events[ACCOUNT_EVENTS];
  size_t event_count, used, attempts;
  unsigned turns;
  bool failed, single;
  char output[ACCOUNT_OUTPUT];
} Account;
bool account_start(Account *a, bool single, bool adversarial_route);
bool account_take(Account *a);
bool account_step(Account *a);
bool account_action(Account *a, unsigned action);
bool account_same(const Account *a, const Account *b);
void account_free(Account *a);
#endif
