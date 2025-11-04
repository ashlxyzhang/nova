#pragma once

#ifndef RENDER_TARGET_HH
#define RENDER_TARGET_HH

#include "pch.hh"

struct RenderTarget
{
        SDL_GPUTexture *texture = nullptr;
        unsigned width = 0;
        unsigned height = 0;
        bool is_focused = false;
};

#endif