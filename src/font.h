#ifndef FONT_H
#define FONT_H

#include <SDL.h>

/* Tiny built-in 5x7 bitmap font: uppercase letters, digits and a few
 * symbols. Lowercase is drawn as uppercase. Uses the current draw color. */
void font_draw(SDL_Renderer *r, float x, float y, float scale, const char *text);

/* Width in pixels of the text once drawn at the given scale. */
float font_width(const char *text, float scale);

#endif
