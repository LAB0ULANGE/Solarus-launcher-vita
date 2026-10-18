#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>
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
#include <solarus/graphics/sdlrenderer/SDLRenderer.h>
#include <solarus/graphics/sdlrenderer/SDLSurfaceImpl.h>
#include <solarus/graphics/sdlrenderer/SDLShader.h>
#include <solarus/graphics/Video.h>
#include <solarus/graphics/Surface.h>
#include <solarus/core/Debug.h>
#include <solarus/graphics/Shader.h>

#include <SDL_render.h>
#include <SDL_hints.h>
#ifdef __vita__
#include <SDL_image.h>
#endif

#ifdef __vita__
extern int solarus_vita_filter_mode;  // Video.cpp: 0 sharp, 1 smooth, 2 scanlines, 3 grid.
#endif

namespace Solarus {

SDLRenderer* SDLRenderer::instance = nullptr;

void SDLRenderer::SurfaceDraw::draw(Surface& dst_surface, const Surface& src_surface, const DrawInfos& params) const {
  SDLRenderer::get().draw(dst_surface.get_impl(),src_surface.get_impl(),params);
}

SDLRenderer::SDLRenderer(SDL_Renderer* a_renderer, bool shaders) : renderer(a_renderer),
  shaders(shaders) {
  if(!renderer) {
    auto rgba_format = Video::get_pixel_format();
    software_screen.reset(SDL_CreateRGBSurface(
        0,
        320,
        240,
        32,
        rgba_format->Rmask,
        rgba_format->Gmask,
        rgba_format->Bmask,
        rgba_format->Amask
    ));
    renderer = SDL_CreateSoftwareRenderer(software_screen.get());

  }
  SOLARUS_REQUIRE(!instance, "Creating two SDL renderer");
  instance = this; //Set this renderer as the unique instance
}

RendererPtr SDLRenderer::create(SDL_Window* window, bool force_software) {
  if(!window) {
    //No window... asked for a software renderer
    return RendererPtr(new SDLRenderer(nullptr,false));
  }

  // Set OpenGL as the default renderer driver when available, to avoid using Direct3d.
#ifndef __vita__
  SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "opengl", SDL_HINT_DEFAULT);
#endif

#ifndef __vita__
  // On Vita, never force SDL's render driver to "software": we want
  // SDL2's own hardware-accelerated (GXM-based) renderer instead.
  if(force_software) {
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
  }
#endif

  // Set the default OpenGL built-in shader (nearest).
  SDL_SetHint(SDL_HINT_RENDER_OPENGL_SHADERS, "1");

#ifdef __vita__
  // Always try the hardware-accelerated (GXM-based) SDL renderer on Vita,
  // regardless of force_software: that flag here only exists to keep
  // GlRenderer/window creation from touching real OpenGL, which is a
  // separate, unrelated concern from SDL2's own Vita renderer backend.
  SDL_Renderer* renderer = SDL_CreateRenderer(
          window,
          -1,
          SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#else
  SDL_Renderer* renderer = force_software ?
        nullptr :
        SDL_CreateRenderer(
          window,
          -1,
          SDL_RENDERER_ACCELERATED);
#endif
  if(not renderer) {
    renderer = SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
  }
  if(not renderer) {
    return nullptr;
  } else {
    //Init shaders
    bool shaders = not force_software and SDLShader::initialize();

    auto size = Video::get_quest_size();
    SDL_RenderSetLogicalSize(renderer,size.width,size.height);
    {
      int ww = 0, wh = 0, ow = 0, oh = 0;
      SDL_GetWindowSize(window, &ww, &wh);
      SDL_GetRendererOutputSize(renderer, &ow, &oh);
      FILE* dbgf = fopen("ux0:data/solarus_debug.log", "a");
      if (dbgf) {
        fprintf(dbgf, "SOLARUS_DEBUG: R window=%dx%d output=%dx%d logical=%dx%d\n",
            ww, wh, ow, oh, size.width, size.height);
        fclose(dbgf);
      }
    }

    return RendererPtr(new SDLRenderer(renderer, shaders));
  }
}

SurfaceImplPtr SDLRenderer::create_texture(int width, int height, int /*margin*/) {
  auto simpl = new SDLSurfaceImpl(renderer,width,height);
  clear(*simpl);
  return SurfaceImplPtr(simpl);
}

SurfaceImplPtr SDLRenderer::create_texture(SDL_Surface_UniquePtr&& surface) {
  return SurfaceImplPtr(new SDLSurfaceImpl(renderer, std::move(surface)));
}

SurfaceImplPtr SDLRenderer::create_window_surface(SDL_Window* /*w*/, int width, int height) {
  return SurfaceImplPtr(new SDLSurfaceImpl(renderer, width, height, true));
}

ShaderPtr SDLRenderer::create_shader(const std::string& shader_id) {
  return std::make_shared<SDLShader>(shader_id);
}

ShaderPtr SDLRenderer::create_shader(const std::string& vertex_source, const std::string& fragment_source, double scaling_factor) {
  return std::make_shared<SDLShader>(
                     vertex_source,
                     fragment_source,
                     scaling_factor);
}

void SDLRenderer::set_render_target(SDL_Texture* target) {
  if(target != render_target || !valid_target) {
    SDL_SetRenderTarget(renderer, target);
    render_target=target;
    valid_target = true;
  }
}

#ifdef __vita__
// Display filters of the Vita (see Video.cpp): a transparent picture drawn
// over the game, made once for the current sizes, so it costs nothing.
static SDL_Texture* vita_filter_texture = nullptr;
static int vita_filter_key[5] = { -1, 0, 0, 0, 0 };

// Part of the screen row (or column) [a, b[ (in game pixels) covered by the
// dark band at the end of each game pixel.
static float vita_band_coverage(double a, double b, double band) {
  double total = 0.0;
  for (int k = static_cast<int>(a) - 1; k <= static_cast<int>(b) + 1; ++k) {
    const double lo = std::max(a, k + 1.0 - band);
    const double hi = std::min(b, k + 1.0);
    if (hi > lo) {
      total += hi - lo;
    }
  }
  return static_cast<float>(total / (b - a));
}

static void vita_draw_filter_overlay(SDL_Renderer* renderer, int game_w, int game_h, const SDL_Rect& dst) {
  const int mode = solarus_vita_filter_mode;
  if (mode != 2 && mode != 3) {
    if (vita_filter_texture != nullptr) {
      SDL_DestroyTexture(vita_filter_texture);
      vita_filter_texture = nullptr;
      vita_filter_key[0] = -1;
    }
    return;
  }
  if (dst.w <= 0 || dst.h <= 0 || game_w <= 0 || game_h <= 0) {
    return;
  }
  if (vita_filter_texture == nullptr || vita_filter_key[0] != mode ||
      vita_filter_key[1] != game_w || vita_filter_key[2] != game_h ||
      vita_filter_key[3] != dst.w || vita_filter_key[4] != dst.h) {
    if (vita_filter_texture != nullptr) {
      SDL_DestroyTexture(vita_filter_texture);
      vita_filter_texture = nullptr;
    }
    // Scanlines: one column is enough (stretched horizontally).
    const int tw = (mode == 2) ? 1 : dst.w;
    const int th = dst.h;
    std::vector<Uint32> pixels(static_cast<size_t>(tw) * th, 0);
    const double sy = static_cast<double>(game_h) / dst.h;
    const double sx = static_cast<double>(game_w) / dst.w;
    for (int y = 0; y < th; ++y) {
      const float cy = vita_band_coverage(y * sy, (y + 1) * sy, mode == 2 ? 0.35 : 0.22);
      for (int x = 0; x < tw; ++x) {
        float darkness;
        if (mode == 2) {
          darkness = cy * 0.55f;
        } else {
          const float cx = vita_band_coverage(x * sx, (x + 1) * sx, 0.22);
          darkness = (1.0f - (1.0f - cy) * (1.0f - cx)) * 0.40f;
        }
        const Uint32 alpha = static_cast<Uint32>(std::min(1.0f, darkness) * 255.0f);
        pixels[static_cast<size_t>(y) * tw + x] = alpha << 24;  // Black, transparent.
      }
    }
    vita_filter_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STATIC, tw, th);
    if (vita_filter_texture == nullptr) {
      return;
    }
    SDL_UpdateTexture(vita_filter_texture, nullptr, pixels.data(), tw * 4);
    SDL_SetTextureBlendMode(vita_filter_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(vita_filter_texture, SDL_ScaleModeNearest);
    vita_filter_key[0] = mode;
    vita_filter_key[1] = game_w;
    vita_filter_key[2] = game_h;
    vita_filter_key[3] = dst.w;
    vita_filter_key[4] = dst.h;
  }
  SDL_RenderCopy(renderer, vita_filter_texture, nullptr, &dst);
}
#endif

void SDLRenderer::draw(SurfaceImpl& dst, const SurfaceImpl& src, const DrawInfos& infos) {
  SDLSurfaceImpl& sdst = dst.as<SDLSurfaceImpl>().targetable();
  const SDLSurfaceImpl& ssrc = src.as<SDLSurfaceImpl>();

  set_render_target(sdst.get_texture());
  Rectangle dst_rect = infos.dst_rectangle();

  // Apply the destination surface's view transform (camera offset).
  // This is necessary for the camera to track entities properly.
  const View& view = dst.get_view();
  glm::vec2 view_center = view.get_center();
  int view_offset_x = static_cast<int>(view_center.x) - dst.get_width() / 2;
  int view_offset_y = static_cast<int>(view_center.y) - dst.get_height() / 2;
  dst_rect.add_x(-view_offset_x);
  dst_rect.add_y(-view_offset_y);

  if(!ssrc.get_texture()) {
    Debug::error("Could not draw screen on another surface");
  }

#ifdef __vita__
  // The game picture drawn on the screen: smooth or sharp pixels.
  const bool vita_to_screen = (sdst.get_texture() == nullptr);
  if (vita_to_screen) {
    SDL_SetTextureScaleMode(ssrc.get_texture(),
        solarus_vita_filter_mode == 1 ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
  }
#endif
  SDL_BlendMode mode = make_sdl_blend_mode(dst,src,infos.blend_mode);
  SOLARUS_CHECK_SDL_HIGHER(SDL_SetTextureBlendMode(ssrc.get_texture(),mode),-1);
  SOLARUS_CHECK_SDL(SDL_SetTextureAlphaMod(ssrc.get_texture(),infos.opacity));
  if(infos.should_use_ex()) {
    SDL_Point origin= infos.sdl_origin();
    SOLARUS_CHECK_SDL(SDL_RenderCopyEx(renderer,ssrc.get_texture(),infos.region,dst_rect,infos.rotation,&origin,infos.flips()));
  } else {
    SOLARUS_CHECK_SDL(SDL_RenderCopy(renderer,ssrc.get_texture(),infos.region,dst_rect));
  }
#ifdef __vita__
  if (vita_to_screen) {
    // Scanlines or LCD grid over the game picture.
    int game_w = 0, game_h = 0;
    SDL_QueryTexture(ssrc.get_texture(), nullptr, nullptr, &game_w, &game_h);
    const SDL_Rect overlay_dst = { dst_rect.get_x(), dst_rect.get_y(), dst_rect.get_width(), dst_rect.get_height() };
    vita_draw_filter_overlay(renderer, game_w, game_h, overlay_dst);
  }
#endif
  sdst.surface_dirty = true;
}

void SDLRenderer::clear(SurfaceImpl& dst) {
  SDLSurfaceImpl& sdst = dst.as<SDLSurfaceImpl>().targetable();
  set_render_target(sdst.get_texture());

  SOLARUS_CHECK_SDL(SDL_SetRenderDrawColor(renderer,0,0,0,0));
  if(sdst.get_texture()) { //texture can be nullptr in case of the screen
    SOLARUS_CHECK_SDL(SDL_SetTextureBlendMode(sdst.get_texture(),SDL_BLENDMODE_BLEND));
  }
  SOLARUS_CHECK_SDL(SDL_RenderClear(renderer));

  sdst.surface_dirty = true;
}

void SDLRenderer::fill(SurfaceImpl& dst, const Color& color, const Rectangle& where, BlendMode mode) {
  SDLSurfaceImpl& sdst = dst.as<SDLSurfaceImpl>().targetable();
  set_render_target(sdst.get_texture());

  Uint8 r,g,b,a;
  color.get_components(r,g,b,a);
  SOLARUS_CHECK_SDL(SDL_SetRenderDrawColor(renderer,r,g,b,a));
  SOLARUS_CHECK_SDL(SDL_SetRenderDrawBlendMode(renderer,make_sdl_blend_mode(mode)));
  SOLARUS_CHECK_SDL(SDL_RenderFillRect(renderer,where));

  sdst.surface_dirty = true;
}

void SDLRenderer::notify_target_changed(const SurfaceImpl& /*surf*/) {

}

void SDLRenderer::invalidate(const SurfaceImpl& surf) {
  const auto& ssurf = surf.as<SDLSurfaceImpl>();

  if(render_target == ssurf.get_texture()) {
    valid_target = false;
  }
}

void SDLRenderer::bind_as_gl_target(SurfaceImpl &surf) {
  SDL_GL_BindTexture(surf.as<SDLSurfaceImpl>().get_texture(),nullptr,nullptr);
}

void SDLRenderer::bind_as_gl_texture(const SurfaceImpl& surf) {
  set_render_target(surf.as<SDLSurfaceImpl>().get_texture());
}

std::string SDLRenderer::get_name() const {
  SDL_RendererInfo renderer_info;
  SDL_GetRendererInfo(renderer, &renderer_info);
  return std::string("SDLRenderer : ") + renderer_info.name;
}

#ifdef __vita__
static bool vita_legend_visible = false;
static std::string vita_legend_text;

void solarus_vita_toggle_legend(const std::string& text) {
  if (vita_legend_visible) {
    vita_legend_visible = false;
  }
  else {
    vita_legend_text = text;
    vita_legend_visible = true;
  }
}

static const char vita_font_chars[] = " 0123456789=?ABCDEFGHIJKLMNOPQRSTUVWXYZ.:-><!/+()_";
static const unsigned char vita_font[50][5] = {
  {0x00,0x00,0x00,0x00,0x00},
  {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},
  {0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
  {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},
  {0x06,0x49,0x49,0x29,0x1E},
  {0x14,0x14,0x14,0x14,0x14},
  {0x02,0x01,0x51,0x09,0x06},
  {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
  {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
  {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
  {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
  {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
  {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
  {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
  {0x00,0x60,0x60,0x00,0x00},{0x00,0x36,0x36,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},
  {0x41,0x22,0x14,0x08,0x00},{0x08,0x14,0x22,0x41,0x00},{0x00,0x00,0x5F,0x00,0x00},
  {0x20,0x10,0x08,0x04,0x02},{0x08,0x08,0x3E,0x08,0x08},{0x00,0x1C,0x22,0x41,0x00},
  {0x00,0x41,0x22,0x1C,0x00},{0x40,0x40,0x40,0x40,0x40}
};

static std::vector<std::string> vita_overlay_lines;
static bool vita_overlay_opaque = false;
// Background of the engine menu: the loading screen picture (app0:loading.png).
// Loaded when the menu opens and freed when it closes, to keep video memory
// free for the games.
static SDL_Texture* vita_menu_background = nullptr;
// False while the player chooses the size or the filter: the game stays
// visible behind the menu to see the result.
bool solarus_vita_menu_show_background = true;
static bool vita_menu_background_failed = false;
void solarus_vita_overlay_opaque(bool opaque) {
  vita_overlay_opaque = opaque;
}
void solarus_vita_overlay_set(const std::vector<std::string>& lines) {
  vita_overlay_lines = lines;
  if (lines.empty()) {
    if (vita_menu_background != nullptr) {
      SDL_DestroyTexture(vita_menu_background);
      vita_menu_background = nullptr;
    }
    vita_menu_background_failed = false;
  }
}

static void vita_draw_text(SDL_Renderer* r, const std::string& text, int x, int y, int scale) {
  for (char c : text) {
    if (c >= 'a' && c <= 'z') {
      c = static_cast<char>(c - 'a' + 'A');
    }
    int index = 12;  // '?' by default
    for (int i = 0; vita_font_chars[i] != 0; ++i) {
      if (vita_font_chars[i] == c) {
        index = i;
        break;
      }
    }
    for (int col = 0; col < 5; ++col) {
      for (int row = 0; row < 7; ++row) {
        if (vita_font[index][col] & (1 << row)) {
          SDL_Rect px = { x + col * scale, y + row * scale, scale, scale };
          SDL_RenderFillRect(r, &px);
        }
      }
    }
    x += 6 * scale;
  }
}

#endif

void SDLRenderer::present(SDL_Window* /*window*/) {
#ifdef __vita__
  if (!vita_overlay_lines.empty()) {
    // The real screen of the Vita (the "output size" given by SDL is the
    // window size, which follows the game's size, e.g. 640x480).
    const int w = 960, h = 544;
    SDL_Rect old_viewport;
    SDL_RenderGetViewport(renderer, &old_viewport);
    float old_sx = 1.0f, old_sy = 1.0f;
    SDL_RenderGetScale(renderer, &old_sx, &old_sy);
    SDL_Rect old_clip;
    SDL_bool clip_enabled = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &old_clip);
    Uint8 or_ = 0, og = 0, ob = 0, oa = 0;
    SDL_GetRenderDrawColor(renderer, &or_, &og, &ob, &oa);
    SDL_BlendMode old_blend;
    SDL_GetRenderDrawBlendMode(renderer, &old_blend);
    // Draw on the whole real screen: no logical size (the game's size),
    // no viewport, no scale while the menu is drawn.
    int old_logical_w = 0, old_logical_h = 0;
    SDL_RenderGetLogicalSize(renderer, &old_logical_w, &old_logical_h);
    SDL_RenderSetLogicalSize(renderer, 0, 0);
    SDL_RenderSetClipRect(renderer, nullptr);
    SDL_Rect whole_screen = { 0, 0, w, h };
    SDL_RenderSetViewport(renderer, &whole_screen);
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);

    int maxlen = 0;
    for (const std::string& l : vita_overlay_lines) {
      int n = static_cast<int>(l.size());
      if (!l.empty() && l[0] == '~') {
        --n;
      }
      if (n > maxlen) {
        maxlen = n;
      }
    }
    const int scale = (maxlen * 12 + 60 <= w) ? 2 : 1;
    const int cw = 6 * scale;
    const int line_h = 7 * scale + 3 * scale;
    const int pw = maxlen * cw + 40;
    const int ph = static_cast<int>(vita_overlay_lines.size()) * line_h + 30;
    const int px0 = (w - pw) / 2;
    int py0 = (h - ph) / 2;
    if (vita_menu_background == nullptr && !vita_menu_background_failed) {
      SDL_Surface* picture = IMG_Load("app0:loading.png");
      if (picture != nullptr) {
        vita_menu_background = SDL_CreateTextureFromSurface(renderer, picture);
        SDL_FreeSurface(picture);
      }
      vita_menu_background_failed = (vita_menu_background == nullptr);
    }
    if (vita_menu_background != nullptr && solarus_vita_menu_show_background) {
      // The text goes below the Solarus logo of the picture.
      py0 = std::max(py0, 250);
      py0 = std::min(py0, h - ph - 16);
      // The loading screen picture, on the whole screen.
      SDL_Rect full = { 0, 0, w, h };
      SDL_RenderCopy(renderer, vita_menu_background, nullptr, &full);
    }
    else if (vita_overlay_opaque) {
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);
    }
    // Text panel: dark purple with a yellow frame (Solarus colors).
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 22, 14, 46, 225);
    SDL_Rect panel = { px0, py0, pw, ph };
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 251, 184, 28, 255);
    SDL_RenderDrawRect(renderer, &panel);
    SDL_Rect frame2 = { px0 + 1, py0 + 1, pw - 2, ph - 2 };
    SDL_RenderDrawRect(renderer, &frame2);
    int y = py0 + 15;
    for (size_t i = 0; i < vita_overlay_lines.size(); ++i) {
      std::string l = vita_overlay_lines[i];
      if (!l.empty() && l[0] == '~') {
        l = l.substr(1);
        SDL_SetRenderDrawColor(renderer, 190, 190, 225, 255);
      }
      else if (i == 0) {
        SDL_SetRenderDrawColor(renderer, 251, 184, 28, 255);
      }
      else if (!l.empty() && l[0] == '>') {
        SDL_SetRenderDrawColor(renderer, 251, 184, 28, 255);
      }
      else {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      }
      vita_draw_text(renderer, l, px0 + 20, y, scale);
      y += line_h;
    }

    SDL_SetRenderDrawColor(renderer, or_, og, ob, oa);
    SDL_SetRenderDrawBlendMode(renderer, old_blend);
    if (old_logical_w > 0 && old_logical_h > 0) {
      SDL_RenderSetLogicalSize(renderer, old_logical_w, old_logical_h);
    }
    SDL_RenderSetViewport(renderer, &old_viewport);
    SDL_RenderSetScale(renderer, old_sx, old_sy);
    SDL_RenderSetClipRect(renderer, clip_enabled ? &old_clip : nullptr);
  }
#endif
#ifdef __vita__
  if (vita_legend_visible) {
    int w = 960, h = 544;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    SDL_Rect old_viewport;
    SDL_RenderGetViewport(renderer, &old_viewport);
    float old_sx = 1.0f, old_sy = 1.0f;
    SDL_RenderGetScale(renderer, &old_sx, &old_sy);
    SDL_Rect old_clip;
    SDL_bool clip_enabled = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &old_clip);
    Uint8 or_ = 0, og = 0, ob = 0, oa = 0;
    SDL_GetRenderDrawColor(renderer, &or_, &og, &ob, &oa);
    SDL_BlendMode old_blend;
    SDL_GetRenderDrawBlendMode(renderer, &old_blend);

    SDL_RenderSetClipRect(renderer, nullptr);
    SDL_RenderSetViewport(renderer, nullptr);
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);

    const int len = static_cast<int>(vita_legend_text.size());
    const int scale = (len * 12 <= w - 20) ? 2 : 1;
    const int cw = 6 * scale;
    const int bar_h = 7 * scale + 12;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_Rect bar = { 0, h - bar_h, w, bar_h };
    SDL_RenderFillRect(renderer, &bar);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    int x = (w - len * cw) / 2;
    const int y = h - bar_h + 6;
    for (char c : vita_legend_text) {
      if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
      }
      int index = 11;  // '?' by default
      for (int i = 0; vita_font_chars[i] != 0; ++i) {
        if (vita_font_chars[i] == c) {
          index = i;
          break;
        }
      }
      for (int col = 0; col < 5; ++col) {
        for (int row = 0; row < 7; ++row) {
          if (vita_font[index][col] & (1 << row)) {
            SDL_Rect px = { x + col * scale, y + row * scale, scale, scale };
            SDL_RenderFillRect(renderer, &px);
          }
        }
      }
      x += cw;
    }

    SDL_SetRenderDrawColor(renderer, or_, og, ob, oa);
    SDL_SetRenderDrawBlendMode(renderer, old_blend);
    SDL_RenderSetViewport(renderer, &old_viewport);
    SDL_RenderSetScale(renderer, old_sx, old_sy);
    SDL_RenderSetClipRect(renderer, clip_enabled ? &old_clip : nullptr);
  }
#endif
  SDL_RenderPresent(renderer);
}

#ifdef __vita__
// R button: toggle between a centered 1:1 view and a full-screen stretch.
static bool vita_stretched = false;
void solarus_vita_toggle_stretch() {
  vita_stretched = !vita_stretched;
}
#endif

void SDLRenderer::on_window_size_changed(const Rectangle& viewport) {
#ifdef __vita__
  if (vita_stretched) {
    // The window surface is drawn over the whole viewport: give it the
    // full 960x544 screen, with no extra scale on top.
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
    SDL_Rect full;
    full.x = 0;
    full.y = 0;
    full.w = 960;
    full.h = 544;
    SDL_RenderSetViewport(renderer, &full);
  } else {
    // Center the display area on the real 960x544 screen.
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
    SDL_Rect centered;
    centered.w = viewport.get_width();
    centered.h = viewport.get_height();
    centered.x = (960 - centered.w) / 2;
    centered.y = (544 - centered.h) / 2;
    SDL_RenderSetViewport(renderer, &centered);
  }
#else
  SDL_RenderSetViewport(renderer,viewport);
#endif
}

/**
 * @brief compute sdl blendmode to use when writing a surface onto another
 * @param dst_surface written to surface
 * @param src_surface read from surface
 * @param blend_mode  solarus blend mode
 * @return a sdl blendmode taking premultiply into account
 */
SDL_BlendMode SDLRenderer::make_sdl_blend_mode(const SurfaceImpl& dst_surface, const SurfaceImpl& src_surface, BlendMode blend_mode) {
  if(dst_surface.is_premultiplied()) { //TODO refactor this a bit
    switch(blend_mode) {
      case BlendMode::NONE:
        return SDL_BLENDMODE_NONE;
      case BlendMode::MULTIPLY:
        return SDL_BLENDMODE_MOD;
      case BlendMode::ADD:
        return SDL_BLENDMODE_ADD;
      case BlendMode::BLEND:
        return SDL_ComposeCustomBlendMode(
              SDL_BLENDFACTOR_SRC_ALPHA,
              SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
              SDL_BLENDOPERATION_ADD,
              SDL_BLENDFACTOR_ONE,
              SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
              SDL_BLENDOPERATION_ADD);
    }
  } else {
    //Straight destination
    if(src_surface.is_premultiplied() && blend_mode == BlendMode::BLEND)
      return SDL_ComposeCustomBlendMode(
            SDL_BLENDFACTOR_ONE,
            SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            SDL_BLENDOPERATION_ADD,
            SDL_BLENDFACTOR_ONE,
            SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            SDL_BLENDOPERATION_ADD);
    switch(blend_mode) { //TODO check other modes
      case BlendMode::NONE:
        return SDL_BLENDMODE_NONE;
      case BlendMode::MULTIPLY:
        return SDL_BLENDMODE_MOD;
      case BlendMode::ADD:
        return SDL_BLENDMODE_ADD;
      case BlendMode::BLEND:
        return SDL_BLENDMODE_BLEND;
    }
  }
  return SDL_BLENDMODE_BLEND;
}

SDL_BlendMode SDLRenderer::make_sdl_blend_mode(BlendMode blend_mode) {
  switch (blend_mode) {
  case BlendMode::ADD:
    return SDL_BLENDMODE_ADD;
  case BlendMode::MULTIPLY:
    return SDL_BLENDMODE_MOD;
  case BlendMode::BLEND:
    return SDL_BLENDMODE_BLEND;
  case BlendMode::NONE:
    return SDL_BLENDMODE_NONE;
  }
  return SDL_BLENDMODE_BLEND;
}

SDLRenderer::~SDLRenderer() {
  SDL_DestroyRenderer(renderer);
}

}
