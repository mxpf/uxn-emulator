#include "mystery.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  const char *script = NULL;
  if (argc == 3 && !strcmp(argv[1], "--script"))
    script = argv[2];
  else if (argc != 1) {
    fprintf(stderr, "usage: mystery [--script 1..9]\n");
    return 1;
  }
  MysterySession *s = calloc(1, sizeof(*s));
  SDL_Window *window = NULL;
  SDL_Renderer *renderer = NULL;
  SDL_Texture *texture = NULL;
  bool ok = false, quit = false;
  Uint32 next = 0;
  if (!s || !mystery_init(s) || SDL_Init(SDL_INIT_VIDEO) != 0)
    goto done;
  window = SDL_CreateWindow(
      "The Last Visitor | Reasoning study | Click objects or 1-9 | R replay | "
      "N restart | Esc quit",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 768,
      script ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!window)
    goto done;
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer)
    goto done;
  if (SDL_RenderSetLogicalSize(renderer, 256, 192))
    goto done;
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING, 256, 192);
  if (!texture)
    goto done;
  if (script) {
    for (size_t i = 0; script[i]; i++) {
      uint8_t in[] = {1, (uint8_t)(script[i] - '0'), 0};
      if (!mystery_input(s, in))
        goto done;
    }
    if (!mystery_replay_begin(s))
      goto done;
    while (s->replaying)
      if (!mystery_replay_step(s))
        goto done;
  }
  do {
    SDL_Event e;
    while (!script && SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        quit = true;
      if (e.type == SDL_KEYDOWN && !e.key.repeat) {
        SDL_Keycode k = e.key.keysym.sym;
        if (k == SDLK_ESCAPE)
          quit = true;
        if (k == SDLK_n) {
          mystery_free(s);
          if (!mystery_init(s))
            goto done;
        } else if (k == SDLK_r && !s->replaying) {
          if (!mystery_replay_begin(s))
            goto done;
          next = SDL_GetTicks() + 450;
        } else if (k >= SDLK_1 && k <= SDLK_9 && !s->replaying) {
          uint8_t in[] = {1, (uint8_t)(k - SDLK_0), 0};
          if (!mystery_input(s, in) && s->failed)
            goto done;
        }
      }
      if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT &&
          !s->replaying) {
        /* SDL logical rendering maps mouse coordinates into the letterboxed
         * canvas. */
        int x = e.button.x, y = e.button.y;
        if (x >= 0 && x < 256 && y >= 0 && y < 192) {
          uint8_t in[] = {0, (uint8_t)x, (uint8_t)y};
          if (!mystery_input(s, in) && s->failed)
            goto done;
        }
      }
    }
    if (!script && s->replaying && (Sint32)(SDL_GetTicks() - next) >= 0) {
      if (!mystery_replay_step(s))
        goto done;
      next = SDL_GetTicks() + 450;
    }
    MysteryMachine *m = s->replaying ? s->play : s->live;
    SDL_SetWindowTitle(
        window, s->replaying ? "The Last Visitor | Replaying | N restart"
                : s->sealed
                    ? "The Last Visitor | Recording full | R replay | N restart"
                : s->replay_done ? "The Last Visitor | Exact replay verified | "
                                   "1-9 inspect | N restart"
                                 : "The Last Visitor | Reasoning study | Click "
                                   "or 1-9 | R replay | N restart "
                                   "| Esc quit");
    if (SDL_UpdateTexture(texture, NULL, m->pixels, 256 * sizeof(uint32_t)) ||
        SDL_RenderClear(renderer) ||
        SDL_RenderCopy(renderer, texture, NULL, NULL))
      goto done;
    SDL_RenderPresent(renderer);
    if (!script)
      SDL_Delay(10);
  } while (!script && !quit);
  printf("%s\nInputs: %zu. Replay: %s.\n", s->live->text, s->count,
         s->replay_done ? "exact" : "not requested");
  ok = true;
done:
  if (!ok)
    fprintf(stderr, "Mystery: %s %s\n", s ? s->error : "Allocation failed.",
            SDL_GetError());
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  mystery_free(s);
  free(s);
  return ok ? 0 : 1;
}
