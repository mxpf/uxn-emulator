#include "mystery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Same small letter shapes as the garden shell; no story strings in the host.
 */
static const uint8_t letters[][7] = {
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30},
    {14, 17, 16, 16, 16, 17, 14}, {30, 17, 17, 17, 17, 17, 30},
    {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17},
    {14, 4, 4, 4, 4, 4, 14},      {7, 2, 2, 2, 2, 18, 12},
    {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17},
    {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16},
    {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},
    {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4},
    {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},
    {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},
    {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
    {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14}};
static const uint32_t palette[] = {0xff111e29, 0xff2b4149, 0xff182a33,
                                   0xff785640, 0xffc69a54, 0xffede0bd,
                                   0xff52757b};
static bool fail(MysterySession *s, const char *text) {
  s->failed = true;
  snprintf(s->error, sizeof(s->error), "%s", text);
  return false;
}

/* Only rectangles, bounded ASCII and a commit. The ROM chooses what to show. */
static void display(Uxn *u, uint8_t port, uint8_t command, void *context) {
  MysteryMachine *m = context;
  (void)port;
  if (!command || m->failed)
    return;
  unsigned x = u->devices[0x20], y = u->devices[0x21], w = u->devices[0x22],
           h = u->devices[0x23], c = u->devices[0x24];
  if (c >= 7) {
    m->failed = true;
    return;
  }
  if (command == 1) {
    for (size_t i = 0; i < MYSTERY_WIDTH * MYSTERY_HEIGHT; i++)
      m->draft[i] = palette[c];
    m->draft_text[0] = 0;
  } else if (command == 2) {
    if (!w || !h || x + w > 256 || y + h > 192) {
      m->failed = true;
      return;
    }
    for (unsigned row = y; row < y + h; row++)
      for (unsigned col = x; col < x + w; col++)
        m->draft[row * 256 + col] = palette[c];
  } else if (command == 3) {
    unsigned address = ((unsigned)u->devices[0x26] << 8) | u->devices[0x27],
             n = u->devices[0x28], cx = x, cy = y;
    if (n > 160 || address > UXN_RAM_SIZE - n) {
      m->failed = true;
      return;
    }
    /* Validate the whole command before drawing, including line extents. */
    for (unsigned i = 0; i < n; i++) {
      uint8_t ch = u->ram[address + i];
      if (ch == 10) {
        cx = x;
        cy += 10;
        continue;
      }
      if (!((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == ' ' ||
            ch == '.' || ch == ':' || ch == '?' || ch == '-') ||
          cx + 5 > 256 || cy + 7 > 192) {
        m->failed = true;
        return;
      }
      cx += 6;
    }
    cx = x;
    cy = y;
    for (unsigned i = 0; i < n; i++) {
      uint8_t ch = u->ram[address + i], extra[7] = {0};
      const uint8_t *g = extra;
      if (ch == 10) {
        cx = x;
        cy += 10;
        continue;
      }
      if (ch >= 'A' && ch <= 'Z')
        g = letters[ch - 'A'];
      else if (ch >= '0' && ch <= '9')
        g = letters[26 + ch - '0'];
      else if (ch == '.')
        extra[6] = 4;
      else if (ch == ':') {
        extra[2] = 4;
        extra[4] = 4;
      } else if (ch == '-')
        extra[3] = 14;
      else if (ch == '?') {
        extra[0] = 14;
        extra[1] = 17;
        extra[2] = 2;
        extra[3] = 4;
        extra[5] = 4;
      }
      for (unsigned row = 0; row < 7; row++)
        for (unsigned col = 0; col < 5; col++)
          if (g[row] & (16 >> col))
            m->draft[(cy + row) * 256 + cx + col] = palette[c];
      cx += 6;
    }
    /* Preserve every ROM-authored text row for the accessible frame. */
    size_t used = strlen(m->draft_text);
    if (used + n + 2 > sizeof(m->draft_text)) {
      m->failed = true;
      return;
    }
    if (used)
      m->draft_text[used++] = '\n';
    memcpy(m->draft_text + used, u->ram + address, n);
    m->draft_text[used + n] = 0;
  } else if (command == 4) {
    memcpy(m->pixels, m->draft, sizeof(m->pixels));
    memcpy(m->text, m->draft_text, sizeof(m->text));
    m->frames++;
  } else
    m->failed = true;
}

static void release(MysteryMachine *m) {
  if (m) {
    runner_free(&m->runner);
    free(m);
  }
}
static MysteryMachine *machine(char *error, size_t capacity) {
  MysteryMachine *m = calloc(1, sizeof(*m));
  uint8_t *buffers[3] = {0};
  RunnerRom roms[3] = {{0}};
  const char *paths[] = {"build/mystery-presentation.rom",
                         "build/mystery-director.rom",
                         "build/mystery-deduction.rom"};
  if (!m) {
    snprintf(error, capacity, "Cannot allocate mystery.");
    return NULL;
  }
  for (unsigned i = 0; i < 3; i++) {
    buffers[i] = malloc(UXN_RAM_SIZE - UXN_ROM_START + 1);
    if (!buffers[i]) {
      snprintf(error, capacity, "Cannot allocate ROM.");
      goto bad;
    }
    FILE *f = fopen(paths[i], "rb");
    if (!f) {
      snprintf(error, capacity, "Cannot open %s. Run from the repository root.",
               paths[i]);
      goto bad;
    }
    size_t n = fread(buffers[i], 1, UXN_RAM_SIZE - UXN_ROM_START + 1, f);
    bool ok = !ferror(f);
    if (fclose(f))
      ok = false;
    if (!ok) {
      snprintf(error, capacity, "Cannot read ROM.");
      goto bad;
    }
    roms[i] = (RunnerRom){buffers[i], n};
  }
  const RoutedRoute routes[] = {
      {ROUTED_NONE, 7, 0}, {0, 1, 1}, {1, 1, 0}, {1, 2, 2}, {2, 1, 1}};
  const RunnerDevice device = {0, 0x25, 0x25, NULL, display, m};
  const RunnerDefinition definition = {roms, 3, routes, 5, &device, 1, 10000};
  RunnerDiagnostic diagnostic;
  if (!runner_start(&m->runner, &definition, &diagnostic)) {
    snprintf(error, capacity, "%s", runner_diagnostic_text(&diagnostic));
    goto bad;
  }
  for (unsigned i = 0; i < 3; i++)
    free(buffers[i]);
  return m;
bad:
  for (unsigned i = 0; i < 3; i++)
    free(buffers[i]);
  release(m);
  return NULL;
}
static bool consume(MysterySession *s, MysteryMachine *m, bool replay) {
  if (m->failed || m->runner.host.fault != ROUTED_OK)
    return fail(s, "Mystery drawing or execution failed.");
  for (unsigned i = 0; i < 3; i++)
    if (m->runner.host.nodes[i].uxn.ram[0xfe])
      return fail(s, "Mystery message could not be sent.");
  RoutedEvent batch[CONSTELLATION_TRACE_CAPACITY];
  size_t n;
  if (!routed_take_trace(&m->runner.host, batch, CONSTELLATION_TRACE_CAPACITY,
                         &n))
    return fail(s, "Cannot read mystery trace.");
  if (replay) {
    if (s->play_event + n > s->event_count ||
        memcmp(s->events + s->play_event, batch, n * sizeof(*batch)))
      return fail(s, "Replay trace differs.");
    s->play_event += n;
  } else {
    if (s->event_count + n > MYSTERY_EVENTS)
      return fail(s, "Mystery trace exceeded its limit.");
    memcpy(s->events + s->event_count, batch, n * sizeof(*batch));
    s->event_count += n;
  }
  return true;
}
static bool settle(MysterySession *s, MysteryMachine *m, bool replay) {
  for (unsigned i = 0; !routed_quiescent(&m->runner.host); i++) {
    if (i == 32)
      return fail(s, "Mystery did not settle in 32 turns.");
    if (!routed_step(&m->runner.host))
      return fail(s, "Mystery execution stopped.");
    m->turns++;
    if (!consume(s, m, replay))
      return false;
  }
  return true;
}
bool mystery_init(MysterySession *s) {
  if (!s || s->live || s->play)
    return false;
  memset(s, 0, sizeof(*s));
  s->live = machine(s->error, sizeof(s->error));
  if (!s->live) {
    s->failed = true;
    return false;
  }
  if (!consume(s, s->live, false) || !settle(s, s->live, false))
    return false;
  return s->live->frames ? true
                         : fail(s, "Mystery ROM did not present a surface.");
}
void mystery_free(MysterySession *s) {
  if (s) {
    release(s->live);
    release(s->play);
    memset(s, 0, sizeof(*s));
  }
}
bool mystery_input(MysterySession *s, const uint8_t input[3]) {
  if (!s || !s->live || s->failed || s->replaying || !input)
    return false;
  if (!((input[0] == 0 && input[2] < 192) ||
        (input[0] == 1 && input[1] >= 1 && input[1] <= 9 && input[2] == 0)))
    return false;
  if (s->count == MYSTERY_INPUTS || s->event_count + 128 > MYSTERY_EVENTS) {
    s->sealed = true;
    snprintf(s->error, sizeof(s->error), "Recording full. Replay or restart.");
    return false;
  }
  if (routed_input(&s->live->runner.host, 7, input, 3) != ROUTED_INPUT_ACCEPTED)
    return fail(s, "Mystery input was rejected.");
  s->replay_done = false;
  memcpy(s->inputs[s->count++], input, 3);
  return consume(s, s->live, false) && settle(s, s->live, false);
}
bool mystery_same(const MysteryMachine *a, const MysteryMachine *b) {
  if (!a || !b || a->failed != b->failed || a->frames != b->frames ||
      a->turns != b->turns || memcmp(a->pixels, b->pixels, sizeof(a->pixels)) ||
      memcmp(a->draft, b->draft, sizeof(a->draft)) ||
      memcmp(a->text, b->text, sizeof(a->text)) ||
      memcmp(a->draft_text, b->draft_text, sizeof(a->draft_text)))
    return false;
  const RoutedHost *h = &a->runner.host, *k = &b->runner.host;
  if (h->node_count != k->node_count || h->route_count != k->route_count ||
      h->next_node != k->next_node || h->trace_sequence != k->trace_sequence ||
      h->trace_count != k->trace_count || h->fault != k->fault ||
      h->booted != k->booted || h->trace_exhausted != k->trace_exhausted ||
      h->ceiling != k->ceiling ||
      memcmp(h->links, k->links, h->route_count * sizeof(*h->links)))
    return false;
  for (size_t i = 0; i < h->node_count; i++) {
    const RoutedNode *x = h->nodes + i, *y = k->nodes + i;
    if (memcmp(x->uxn.ram, y->uxn.ram, sizeof(x->uxn.ram)) ||
        memcmp(x->uxn.devices, y->uxn.devices, sizeof(x->uxn.devices)) ||
        memcmp(&x->uxn.working, &y->uxn.working, sizeof(UxnStack)) ||
        memcmp(&x->uxn.return_stack, &y->uxn.return_stack, sizeof(UxnStack)) ||
        x->uxn.instructions != y->uxn.instructions || x->loaded != y->loaded ||
        x->id != y->id || x->source != y->source ||
        x->writable != y->writable || x->next_incoming != y->next_incoming ||
        x->next_writable != y->next_writable ||
        x->prefer_receive != y->prefer_receive)
      return false;
  }
  return true;
}
bool mystery_replay_begin(MysterySession *s) {
  if (!s || !s->live || s->failed || s->replaying)
    return false;
  release(s->play);
  s->play = machine(s->error, sizeof(s->error));
  s->play_event = s->play_input = 0;
  s->replay_done = false;
  if (!s->play) {
    s->failed = true;
    return false;
  }
  s->replaying = true;
  return consume(s, s->play, true) && settle(s, s->play, true);
}
bool mystery_replay_step(MysterySession *s) {
  if (!s || s->failed || !s->replaying)
    return false;
  if (s->play_input < s->count) {
    const uint8_t *input = s->inputs[s->play_input++];
    if (routed_input(&s->play->runner.host, 7, input, 3) !=
        ROUTED_INPUT_ACCEPTED)
      return fail(s, "Replay input differs.");
    if (!consume(s, s->play, true) || !settle(s, s->play, true))
      return false;
  }
  if (s->play_input == s->count) {
    if (s->play_event != s->event_count || !mystery_same(s->live, s->play))
      return fail(s, "Replay final state differs.");
    s->replay_done = true;
    s->replaying = false;
  }
  return true;
}
