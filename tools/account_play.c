#include "account.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif
static Account *current;
static uint8_t log_actions[ACCOUNT_INPUTS];
static unsigned count;
EXPORT int study_reset(int single) {
  if (current) {
    account_free(current);
    free(current);
  }
  current = calloc(1, sizeof(*current));
  count = 0;
  return current && account_start(current, single != 0, false);
}
EXPORT int study_action(int action) {
  if (!current || count >= ACCOUNT_INPUTS ||
      !account_action(current, (unsigned)action))
    return 0;
  log_actions[count++] = (uint8_t)action;
  return 1;
}
EXPORT const char *study_output(void) { return current ? current->output : ""; }
EXPORT int study_replay(void) {
  if (!current)
    return 0;
  Account *p = calloc(1, sizeof(*p));
  bool ok = p && account_start(p, current->single, false);
  for (unsigned i = 0; ok && i < count; i++)
    ok = account_action(p, log_actions[i]);
  ok = ok && account_same(current, p);
  if (p) {
    account_free(p);
    free(p);
  }
  return ok;
}
EXPORT const char *study_trace(void) {
  static char text[180000];
  size_t used = 0;
  text[0] = 0;
  if (!current)
    return text;
  for (size_t i = 0; i < current->event_count; i++) {
    const RoutedEvent *e = current->events + i;
    int n = snprintf(text + used, sizeof(text) - used,
                     "%" PRIu64 " kind=%u %02x->%02x route=%02x", e->sequence,
                     (unsigned)e->kind, e->from, e->to, e->selector);
    if (n < 0 || (size_t)n >= sizeof(text) - used)
      break;
    used += (size_t)n;
    for (unsigned j = 0; j < e->message.length && used + 4 < sizeof(text); j++)
      used += (size_t)snprintf(text + used, sizeof(text) - used, " %02x",
                               e->message.data[j]);
    if (used + 2 >= sizeof(text))
      break;
    text[used++] = '\n';
    text[used] = 0;
  }
  return text;
}
#ifndef __EMSCRIPTEN__
int main(int argc, char **argv) {
  if (argc < 2 || (strcmp(argv[1], "single") && strcmp(argv[1], "multi"))) {
    fprintf(stderr, "Usage: account-study multi|single [12342] "
                    "[--trace]\nWithout a script, enter actions 1 read, 2 ask, "
                    "3 return, 4 show; R replays; Q quits.\n");
    return 2;
  }
  if (!study_reset(!strcmp(argv[1], "single")))
    return 1;
  if (argc >= 3) {
    for (const char *p = argv[2]; *p; p++)
      if (!study_action(*p - '0'))
        return 1;
    fputs(study_output(), stdout);
    if (!study_replay())
      return 1;
    puts("Replay: exact.");
    if (argc == 4 && !strcmp(argv[3], "--trace"))
      fputs(study_trace(), stdout);
  } else {
    size_t used = 0;
    int ch;
    puts("1 read record; 2 ask Bram; 3 return page; 4 show entry; R replay; Q "
         "quit.");
    fputs(study_output(), stdout);
    used = current->used;
    while ((ch = getchar()) != EOF && ch != 'q' && ch != 'Q') {
      if (ch == 'r' || ch == 'R') {
        puts(study_replay() ? "Replay: exact." : "Replay failed.");
        continue;
      }
      if (ch == '\n' || ch == ' ')
        continue;
      if (!study_action(ch - '0')) {
        puts("Input refused (invalid or recording limit).");
        continue;
      }
      fputs(study_output() + used, stdout);
      used = current->used;
    }
  }
  account_free(current);
  free(current);
  return 0;
}
#endif
