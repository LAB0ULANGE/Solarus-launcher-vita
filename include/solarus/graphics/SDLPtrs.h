/*
 * Copyright (C) 2018-2020 std::gregwar, Solarus - http://www.solarus-games.org
 *
 * Solarus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Solarus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <SDL_render.h>
#include <memory>

extern "C" int solarus_vita_live_tex;
extern "C" long long solarus_vita_live_bytes;

struct SDL_Texture_Deleter {
    void operator()(SDL_Texture* texture) const {
      --solarus_vita_live_tex;
      { int qw = 0, qh = 0; SDL_QueryTexture(texture, nullptr, nullptr, &qw, &qh); solarus_vita_live_bytes -= (long long)qw * qh * 4; }
      SDL_DestroyTexture(texture);
    }
};
using SDL_Texture_UniquePtr = std::unique_ptr<SDL_Texture, SDL_Texture_Deleter>;

struct SDL_Surface_Deleter {
     void operator()(SDL_Surface* sdl_surface) const {
       SDL_FreeSurface(sdl_surface);
     }
 };
 using SDL_Surface_UniquePtr = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>;
