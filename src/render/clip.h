#pragma once

#include <SDL3/SDL.h>

namespace gt3r::render {

// Under a viewport that starts off the target (the app's 4:3 box, App::layout_ui), SDL's GPU
// renderer turns a clip off with a scissor at the viewport's negative pixel position, and a
// clip left of the target becomes a negative scissor too; Vulkan does not allow either (phones
// dropped every draw after it). There, the visible part of the viewport (viewport units, 1 unit
// in from an edge off the target, for SDL's separate rounding of the two offsets).
inline bool visible_area(SDL_Renderer* r, SDL_Rect* area) {
    SDL_Rect vp;
    int lw = 0, lh = 0;
    SDL_RendererLogicalPresentation mode;
    if (!SDL_RenderViewportSet(r) || !SDL_GetRenderViewport(r, &vp) || (vp.x >= 0 && vp.y >= 0) ||
        !SDL_GetRenderLogicalPresentation(r, &lw, &lh, &mode) || mode == SDL_LOGICAL_PRESENTATION_DISABLED)
        return false;
    const int x0 = vp.x < 0 ? 1 - vp.x : 0, y0 = vp.y < 0 ? 1 - vp.y : 0;
    *area = {x0, y0, lw - (vp.x < 0 ? 2 : 0), lh - (vp.y < 0 ? 2 : 0)};
    return true;
}

// Turns the clip off (there: clips to the visible part).
inline void clip_off(SDL_Renderer* r) {
    SDL_Rect area;
    SDL_SetRenderClipRect(r, visible_area(r, &area) ? &area : nullptr);
}

// Sets a clip rectangle, kept inside that visible part (the same reason).
inline void clip_set(SDL_Renderer* r, const SDL_Rect& clip) {
    SDL_Rect area, both = clip;
    if (visible_area(r, &area) && !SDL_GetRectIntersection(&clip, &area, &both)) both = {area.x, area.y, 0, 0};
    SDL_SetRenderClipRect(r, &both);
}

// Restores a clip saved with SDL_RenderClipEnabled / SDL_GetRenderClipRect.
inline void clip_restore(SDL_Renderer* r, bool had_clip, const SDL_Rect& old_clip) {
    if (had_clip) clip_set(r, old_clip);
    else clip_off(r);
}

} // namespace gt3r::render
