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
#include "solarus/graphics/sdlrenderer/SDLSurfaceImpl.h"
#include "solarus/graphics/sdlrenderer/SDLRenderer.h"
#include "solarus/core/Debug.h"
#include "solarus/graphics/Video.h"
#include "solarus/core/QuestFiles.h"
#include <SDL_render.h>
#include <cstdio>
#include <SDL_image.h>
#ifdef __vita__
#include <psp2/kernel/sysmem.h>
#include <malloc.h>
#endif

namespace Solarus {

void (*solarus_vita_gc_hook)() = nullptr;
extern "C" { int solarus_vita_live_tex = 0; long long solarus_vita_live_bytes = 0; }

SDL_Texture* create_texture_from_renderer(SDL_Renderer* renderer, int width, int height) {
  SDL_PixelFormat* format = Video::get_pixel_format();

  SOLARUS_REQUIRE(renderer != nullptr, "Missing renderer");
  SOLARUS_REQUIRE(format != nullptr, "Missing RGBA pixel format");

  SDL_Texture* tex = SDL_CreateTexture(
      renderer,
      Video::get_pixel_format()->format,
      SDL_TEXTUREACCESS_TARGET,
      width,
      height);
#ifdef __vita__
  if (tex == nullptr && solarus_vita_gc_hook != nullptr) {
    solarus_vita_gc_hook();
    solarus_vita_gc_hook();
    tex = SDL_CreateTexture(
        renderer,
        Video::get_pixel_format()->format,
        SDL_TEXTUREACCESS_TARGET,
        width,
        height);
  }
#endif
#ifdef __vita__
  {
    FILE* texlog = (tex != nullptr) ? nullptr : fopen("ux0:data/solarus_debug.log", "a");
    if (texlog) {
      SceKernelFreeMemorySizeInfo mi;
      mi.size = sizeof(mi);
      sceKernelGetFreeMemorySize(&mi);
      struct mallinfo mm = mallinfo();
      fprintf(texlog, "TEX vivantes=%d (%dK) target %dx%d %s | libre: user=%dK cdram=%dK phycont=%dK | tas: utilise=%dK arena=%dK\n", solarus_vita_live_tex, (int)(solarus_vita_live_bytes / 1024), width, height, tex ? "ok" : "FAILED", mi.size_user / 1024, mi.size_cdram / 1024, mi.size_phycont / 1024, (int)(mm.uordblks / 1024), (int)(mm.arena / 1024));
      fclose(texlog);
    }
  }
#endif
  if (tex != nullptr) { ++solarus_vita_live_tex; int qw = 0, qh = 0; SDL_QueryTexture(tex, nullptr, nullptr, &qw, &qh); solarus_vita_live_bytes += (long long)qw * qh * 4; }
  SOLARUS_REQUIRE(tex != nullptr,
      std::string("Failed to create render texture ") + std::to_string(width) + "x" +
      std::to_string(height) + " : " + SDL_GetError());
  return tex;
}

SDLSurfaceImpl::SDLSurfaceImpl(SDL_Renderer *renderer, int width, int height, bool screen_tex) :
  SurfaceImpl({width, height}),
  target(true) {
  texture.reset(screen_tex ? nullptr : create_texture_from_renderer(renderer,width,height));

  SDL_PixelFormat* format = Video::get_pixel_format();
  SDL_Surface* surf_ptr = SDL_CreateRGBSurface(
       0,
       width,
       height,
       32,
       format->Rmask,
       format->Gmask,
       format->Bmask,
       format->Amask);
  SOLARUS_REQUIRE(surf_ptr != nullptr,
      std::string("Failed to create backup surface ") + SDL_GetError());
  surface.reset(surf_ptr);
}

SDLSurfaceImpl::SDLSurfaceImpl(SDL_Renderer* renderer, SDL_Surface_UniquePtr surface)
  : SurfaceImpl({surface->w, surface->h}),
    target(false), surface(std::move(surface)) {
  SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, this->surface.get());
  if (tex != nullptr) { ++solarus_vita_live_tex; int qw = 0, qh = 0; SDL_QueryTexture(tex, nullptr, nullptr, &qw, &qh); solarus_vita_live_bytes += (long long)qw * qh * 4; }
  SOLARUS_REQUIRE(tex != nullptr,
      std::string("Failed to convert surface to texture") + SDL_GetError());
  texture.reset(tex);
}

/**
 * @brief upload potentially modified surface
 *
 * When modifying pixels of the Surface, we have
 * to upload it to the texture for changes to be reflected
 */
void SDLSurfaceImpl::upload_surface() {
  Rectangle rect(0,0,get_width(),get_height());
  SDL_Surface* surface = get_surface();
  SDL_UpdateTexture(get_texture(),
                    rect,
                    surface->pixels,
                    surface->pitch
                    );
}

void SDLSurfaceImpl::release_cpu_copy(const std::string& file_name) {
  if (!target && surface != nullptr) {
    source_file = file_name;
    surface.reset();
  }
}

/**
 * \copydoc SurfaceImpl::get_texture
 */
SDL_Texture* SDLSurfaceImpl::get_texture() const {
    return texture.get();
}

/**
 * \copydoc SurfaceImpl::get_surface
 */
SDL_Surface* SDLSurfaceImpl::get_surface() const {
  if (!surface && !target && !source_file.empty()) {
    // The CPU copy was released: reload it from the quest file.
    const std::string& buffer = QuestFiles::data_file_read(source_file);
    SDL_RWops* rw = SDL_RWFromMem(const_cast<char*>(buffer.data()), (int) buffer.size());
    SDL_Surface_UniquePtr loaded = SDL_Surface_UniquePtr(IMG_Load_RW(rw, 0));
    SDL_RWclose(rw);
    SOLARUS_REQUIRE(loaded != nullptr,
        std::string("Cannot reload image '") + source_file + "': " + SDL_GetError());
    SDL_PixelFormat* pixel_format = Video::get_pixel_format();
    if (loaded->format->format != pixel_format->format) {
      SDL_Surface_UniquePtr converted = SDL_Surface_UniquePtr(
          SDL_ConvertSurface(loaded.get(), pixel_format, 0));
      SOLARUS_REQUIRE(converted != nullptr,
          std::string("Failed to convert software surface: ") + SDL_GetError());
      loaded = std::move(converted);
    }
    surface = std::move(loaded);
  }
  if (target and surface_dirty) {
    SDLRenderer::get().set_render_target(get_texture());
    SOLARUS_CHECK_SDL(SDL_RenderReadPixels(SDLRenderer::get().renderer,
                         NULL,
                         Video::get_pixel_format()->format,
                         surface->pixels,
                         surface->pitch
                         ));
    surface_dirty = false;
  }
  return surface.get();
}

SDLSurfaceImpl& SDLSurfaceImpl::targetable()  {
  if(target) {
    surface_dirty = true;
    return *this;
  } else if(texture) { //Don't create texture for screen special case
    //Recreate texture
    auto& r = SDLRenderer::get();
    SDL_Texture* tex = create_texture_from_renderer(r.renderer,get_width(),get_height());
    r.set_render_target(tex);
    SDL_SetTextureBlendMode(get_texture(),SDL_BLENDMODE_NONE);
    SDL_RenderCopy(r.renderer,get_texture(),nullptr,nullptr);
    texture.reset(tex);
  }
  return *this;
}

}
