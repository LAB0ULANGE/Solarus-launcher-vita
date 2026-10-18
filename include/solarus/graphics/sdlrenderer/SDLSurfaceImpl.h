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

#include "solarus/core/Rectangle.h"
#include "solarus/core/Point.h"
#include "solarus/graphics/SDLPtrs.h"
#include "solarus/graphics/SurfaceImpl.h"

#include <SDL_render.h>
#include <memory>
#include <string>

namespace Solarus {

class SDLRenderer;
/**
 * @brief Abstract class for internal surface pixel representation and manipulation
 */
class SDLSurfaceImpl : public SurfaceImpl
{
  friend class SDLRenderer;
public:
  SDLSurfaceImpl(SDL_Renderer* renderer, int width, int height, bool screen_tex = false);
  SDLSurfaceImpl(SDL_Renderer* renderer, SDL_Surface_UniquePtr surface);

  SDL_Texture* get_texture() const;
  SDL_Surface* get_surface() const override;

  SDLSurfaceImpl& targetable();

  /**
   * @brief upload potentially modified surface
   *
   * When modifying pixels of the Surface, we have
   * to upload it to the texture for changes to be reflected
   */
  void upload_surface() override;

  // Frees the CPU copy of an image that is only drawn (it is reloaded
  // from the quest file if some code asks for the pixels later).
  void release_cpu_copy(const std::string& file_name);
private:
  std::string source_file;
  bool target = false;
  mutable bool surface_dirty = true;
  mutable SDL_Texture_UniquePtr texture;
  mutable SDL_Surface_UniquePtr surface;
};

}
