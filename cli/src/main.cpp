#ifdef __vita__
#include <psp2/kernel/processmgr.h>
#endif
/*
 * Copyright (C) 2006-2019 Christopho, Solarus - http://www.solarus-games.org
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
#include "solarus/core/Arguments.h"
#include "solarus/core/Debug.h"
#include "solarus/core/MainLoop.h"
#include <iostream>
#include <string>

// SDLmain is required in some platforms, i.e. Windows, for proper initialization.
// For instance, in Windows, SDLmain encodes argv in main() using UTF-8 by default.
#include <SDL.h>


#ifdef __vita__
// VITA_EARLY_SPLASH: the loading screen is drawn straight to the Vita display
// at the very start of the program, before the quest archive is opened and
// before SDL starts (that takes several seconds on big games).
#include <psp2/display.h>
#include <psp2/power.h>
#include <SDL.h>
#include <SDL_image.h>
#include <psp2/kernel/sysmem.h>
#include <cstring>
#include <algorithm>
#include <vector>
namespace {
const char splash_font_chars[] = " 0123456789=?ABCDEFGHIJKLMNOPQRSTUVWXYZ.:-><!/+()_";
const unsigned char splash_font[50][5] = {
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
const int SPLASH_W = 960;
const int SPLASH_H = 544;
SceUID splash_uid = -1;

// Pixel format A8B8G8R8: 0xAABBGGRR in memory order R, G, B, A.
inline unsigned int splash_rgb(int r, int g, int b) {
  return 0xFF000000u | (static_cast<unsigned>(b) << 16) | (static_cast<unsigned>(g) << 8) | static_cast<unsigned>(r);
}

void splash_rect(unsigned int* fb, int x, int y, int w, int h, unsigned int color) {
  for (int j = y; j < y + h; ++j) {
    if (j < 0 || j >= SPLASH_H) continue;
    for (int i = x; i < x + w; ++i) {
      if (i < 0 || i >= SPLASH_W) continue;
      fb[j * SPLASH_W + i] = color;
    }
  }
}

void splash_text(unsigned int* fb, const std::string& text, int x, int y, int scale, unsigned int color) {
  for (char c : text) {
    if (c >= 'a' && c <= 'z') {
      c = static_cast<char>(c - 'a' + 'A');
    }
    int index = 12;
    for (int i = 0; splash_font_chars[i] != 0; ++i) {
      if (splash_font_chars[i] == c) {
        index = i;
        break;
      }
    }
    for (int col = 0; col < 5; ++col) {
      for (int row = 0; row < 7; ++row) {
        if (splash_font[index][col] & (1 << row)) {
          splash_rect(fb, x + col * scale, y + row * scale, scale, scale, color);
        }
      }
    }
    x += 6 * scale;
  }
}

// Thumbnail of the game: ux0:data/Solarus/thumbs/<quest file name>.png,
// drawn centered in a box, keeping its proportions. Returns false if absent.
bool splash_draw_thumbnail(unsigned int* fb, const std::string& name, int box_x, int box_y, int box_w, int box_h) {
  if (name.empty()) {
    return false;
  }
  const std::string path = "ux0:data/Solarus/thumbs/" + name + ".png";
  SDL_Surface* img = IMG_Load(path.c_str());
  if (img == nullptr) {
    return false;
  }
  SDL_Surface* rgba = SDL_ConvertSurfaceFormat(img, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(img);
  if (rgba == nullptr) {
    return false;
  }
  int w = box_w;
  int h = rgba->h * box_w / rgba->w;
  if (h > box_h) {
    h = box_h;
    w = rgba->w * box_h / rgba->h;
  }
  const int x0 = box_x + (box_w - w) / 2;
  const int y0 = box_y + (box_h - h) / 2;
  splash_rect(fb, x0 - 3, y0 - 3, w + 6, h + 6, splash_rgb(255, 255, 255));
  SDL_LockSurface(rgba);
  const unsigned char* pixels = static_cast<const unsigned char*>(rgba->pixels);
  for (int y = 0; y < h; ++y) {
    const unsigned int* src_row = reinterpret_cast<const unsigned int*>(pixels + (y * rgba->h / h) * rgba->pitch);
    for (int x = 0; x < w; ++x) {
      fb[(y0 + y) * SPLASH_W + x0 + x] = src_row[x * rgba->w / w] | 0xFF000000u;
    }
  }
  SDL_UnlockSurface(rgba);
  SDL_FreeSurface(rgba);
  return true;
}

// Text with a dark outline, centered on the screen.
void splash_text_centered(unsigned int* fb, const std::string& text, int y, unsigned int color) {
  const int tx = (SPLASH_W - static_cast<int>(text.size()) * 12) / 2;
  for (int dy = -2; dy <= 2; dy += 2) {
    for (int dx = -2; dx <= 2; dx += 2) {
      splash_text(fb, text, tx + dx, y + dy, 2, splash_rgb(0, 0, 0));
    }
  }
  splash_text(fb, text, tx, y, 2, color);
}

// Name of the game about to start (empty for the game list).
std::string splash_game_name() {
  FILE* mf = fopen("ux0:data/Solarus/mode.txt", "r");
  if (mf == nullptr) {
    return "";
  }
  char mode[16] = { 0 };
  const bool play = fgets(mode, sizeof(mode), mf) != nullptr && std::strncmp(mode, "play", 4) == 0;
  fclose(mf);
  if (!play) {
    return "";
  }
  FILE* qf = fopen("ux0:data/Solarus/quest.txt", "r");
  if (qf == nullptr) {
    return "";
  }
  char line[512] = { 0 };
  std::string n;
  if (fgets(line, sizeof(line), qf) != nullptr) {
    n = line;
  }
  fclose(qf);
  while (!n.empty() && (n.back() == '\n' || n.back() == '\r' || n.back() == ' ' || n.back() == '/')) {
    n.pop_back();
  }
  const size_t slash = n.find_last_of("/\\:");
  if (slash != std::string::npos) {
    n = n.substr(slash + 1);
  }
  const size_t dot = n.rfind('.');
  if (dot != std::string::npos && dot > 0) {
    n = n.substr(0, dot);
  }
  if (n.size() > 30) {
    n = n.substr(0, 30);
  }
  return n;
}

unsigned int* splash_fb = nullptr;
std::vector<unsigned int> splash_background;  // Copy of the screen without the bar.
bool splash_has_image = false;
const int BAR_X = 120;
const int BAR_W = SPLASH_W - 2 * BAR_X;
const int BAR_Y = SPLASH_H - 56;
const int BAR_H = 14;

// VITA_FUN_MESSAGES: random funny messages under the progress bar.
const char* const splash_messages[] = {
  "CHARGEMENT DES PIXELS...",
  "HABILLAGE DU HEROS EN VERT...",
  "AFFUTAGE DE LA LAME...",
  "REMPLISSAGE DES COEURS...",
  "CACHETTE DES RUBIS DANS LES HERBES...",
  "REVEIL DES POULES (PRUDENCE)...",
  "POLISSAGE DES BOUCLIERS...",
  "ARROSAGE DES BUISSONS...",
  "PLACEMENT DES POTS A CASSER...",
  "ENTRAINEMENT DES CHAUVES-SOURIS...",
  "CALIBRAGE DU BOOMERANG...",
  "BALAYAGE DES DONJONS...",
  "MISE EN PLACE DES COFFRES...",
  "CUISSON DES POTIONS ROUGES...",
  "ALLUMAGE DES TORCHES...",
  "DEPLIAGE DE LA CARTE DU MONDE...",
  "RANGEMENT DES CLES DE DONJON...",
  "REVEIL DES FEES...",
  "RECHAUFFAGE DES PIXELS...",
  "GONFLAGE DES OCTOROKS...",
  "ACCORDAGE DE LA MUSIQUE EPIQUE...",
  "COMPTAGE DES RUBIS...",
  "CIRAGE DES BOTTES DE PEGASE...",
  "DRESSAGE DES MOBLINS...",
  "RECHERCHE DE LA PRINCESSE...",
};
const int splash_message_count = sizeof(splash_messages) / sizeof(splash_messages[0]);
const int MSG_Y = SPLASH_H - 30;
std::vector<unsigned int> splash_message_area;  // Background under the message.
unsigned long long splash_message_time = 0;
int splash_message_index = -1;

void splash_save_message_area(const unsigned int* fb) {
  splash_message_area.assign(fb + (MSG_Y - 3) * SPLASH_W, fb + (MSG_Y + 17) * SPLASH_W);
}

void splash_draw_message(unsigned int* fb) {
  if (splash_message_area.empty()) {
    return;
  }
  int index = static_cast<int>((sceKernelGetProcessTimeWide() / 1000) % splash_message_count);
  if (index == splash_message_index) {
    index = (index + 1) % splash_message_count;
  }
  splash_message_index = index;
  std::copy(splash_message_area.begin(), splash_message_area.end(), fb + (MSG_Y - 3) * SPLASH_W);
  const std::string text = splash_messages[index];
  const int tx = (SPLASH_W - static_cast<int>(text.size()) * 12) / 2;
  for (int dy = -2; dy <= 2; dy += 2) {
    for (int dx = -2; dx <= 2; dx += 2) {
      splash_text(fb, text, tx + dx, MSG_Y + dy, 2, splash_rgb(0, 0, 0));
    }
  }
  splash_text(fb, text, tx, MSG_Y, 2, splash_rgb(200, 200, 200));
  splash_message_time = sceKernelGetProcessTimeWide();
}

// Loads app0:loading.png (any size, scaled to 960 x 544) into the frame.
bool splash_draw_image(unsigned int* fb) {
  SDL_Surface* img = IMG_Load("app0:loading.png");
  {
    FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
    if (dbg) {
      fprintf(dbg, "SPLASH image app0:loading.png : %s %s\n", img ? "chargee" : "ECHEC", img ? "" : SDL_GetError());
      fclose(dbg);
    }
  }
  if (img == nullptr) {
    return false;
  }
  SDL_Surface* rgba = SDL_ConvertSurfaceFormat(img, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(img);
  if (rgba == nullptr) {
    return false;
  }
  SDL_LockSurface(rgba);
  const unsigned char* pixels = static_cast<const unsigned char*>(rgba->pixels);
  for (int y = 0; y < SPLASH_H; ++y) {
    const int sy = y * rgba->h / SPLASH_H;
    const unsigned int* src_row = reinterpret_cast<const unsigned int*>(pixels + sy * rgba->pitch);
    for (int x = 0; x < SPLASH_W; ++x) {
      const int sx = x * rgba->w / SPLASH_W;
      fb[y * SPLASH_W + x] = src_row[sx] | 0xFF000000u;
    }
  }
  SDL_UnlockSurface(rgba);
  SDL_FreeSurface(rgba);
  return true;
}

// Text panel, used when there is no loading.png.
void splash_draw_panel(unsigned int* fb, const std::string& name) {
  std::vector<std::string> lines;
  lines.push_back("SOLARUS");
  lines.push_back("");
  lines.push_back("  CHARGEMENT...");
  if (!name.empty()) {
    lines.push_back("");
    lines.push_back("~" + name);
  }
  int maxlen = 0;
  for (const std::string& l : lines) {
    int n = static_cast<int>(l.size());
    if (!l.empty() && l[0] == '~') {
      --n;
    }
    if (n > maxlen) {
      maxlen = n;
    }
  }
  const int scale = (maxlen * 12 + 60 <= SPLASH_W) ? 2 : 1;
  const int cw = 6 * scale;
  const int line_h = 7 * scale + 3 * scale;
  const int pw = maxlen * cw + 40;
  const int ph = static_cast<int>(lines.size()) * line_h + 30;
  const int px0 = (SPLASH_W - pw) / 2;
  const int py0 = (SPLASH_H - ph) / 2 - 30;
  const unsigned int white = splash_rgb(255, 255, 255);
  splash_rect(fb, px0, py0, pw, 1, white);
  splash_rect(fb, px0, py0 + ph - 1, pw, 1, white);
  splash_rect(fb, px0, py0, 1, ph, white);
  splash_rect(fb, px0 + pw - 1, py0, 1, ph, white);
  int y = py0 + 15;
  for (size_t i = 0; i < lines.size(); ++i) {
    std::string l = lines[i];
    unsigned int color = white;
    if (!l.empty() && l[0] == '~') {
      l = l.substr(1);
      color = splash_rgb(170, 170, 170);
    }
    else if (i == 0) {
      color = splash_rgb(90, 220, 255);
    }
    splash_text(fb, l, px0 + 20, y, scale, color);
    y += line_h;
  }
}

void splash_show() {
  const unsigned int size = 2 * 1024 * 1024;  // 960 x 544 x 4, rounded up.
  splash_uid = sceKernelAllocMemBlock("vita_splash", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, size, nullptr);
  if (splash_uid < 0) {
    splash_uid = -1;
    return;
  }
  void* base = nullptr;
  if (sceKernelGetMemBlockBase(splash_uid, &base) < 0 || base == nullptr) {
    sceKernelFreeMemBlock(splash_uid);
    splash_uid = -1;
    return;
  }
  unsigned int* fb = static_cast<unsigned int*>(base);
  splash_fb = fb;
  for (int i = 0; i < SPLASH_W * SPLASH_H; ++i) {
    fb[i] = splash_rgb(0, 0, 0);
  }

  const std::string name = splash_game_name();
  splash_has_image = splash_draw_image(fb);
  if (splash_has_image) {
    if (!name.empty()) {
      // The game's thumbnail if the launcher saved one, else its name.
      if (!splash_draw_thumbnail(fb, name, 330, 250, 300, 170)) {
        splash_text_centered(fb, name, 340, splash_rgb(255, 255, 255));
      }
      // Tell the player how to come back to the game list.
      splash_text_centered(fb, "SELECT + L = AFFICHER LE MENU SOLARUS", BAR_Y - 30, splash_rgb(251, 184, 28));
    }
  }
  else {
    splash_draw_panel(fb, name);
  }

  // Empty progress bar.
  const unsigned int white = splash_rgb(255, 255, 255);
  splash_rect(fb, BAR_X - 3, BAR_Y - 3, BAR_W + 6, BAR_H + 6, splash_rgb(0, 0, 0));
  splash_rect(fb, BAR_X - 2, BAR_Y - 2, BAR_W + 4, 1, white);
  splash_rect(fb, BAR_X - 2, BAR_Y + BAR_H + 1, BAR_W + 4, 1, white);
  splash_rect(fb, BAR_X - 2, BAR_Y - 2, 1, BAR_H + 4, white);
  splash_rect(fb, BAR_X + BAR_W + 1, BAR_Y - 2, 1, BAR_H + 4, white);

  splash_save_message_area(fb);
  splash_draw_message(fb);

  SceDisplayFrameBuf frame;
  std::memset(&frame, 0, sizeof(frame));
  frame.size = sizeof(frame);
  frame.base = base;
  frame.pitch = SPLASH_W;
  frame.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
  frame.width = SPLASH_W;
  frame.height = SPLASH_H;
  sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
}
}  // namespace

// True while the early loading screen is on the display.
bool solarus_vita_splash_active() {
  return splash_uid >= 0;
}

// Fills the progress bar (0 to 1). Never goes back.
void solarus_vita_splash_progress(float progress) {
  static int filled = 0;
  if (splash_fb == nullptr) {
    return;
  }
  if (progress < 0.0f) progress = 0.0f;
  if (progress > 1.0f) progress = 1.0f;
  const int target = static_cast<int>(progress * BAR_W);
  if (target <= filled) {
    if (sceKernelGetProcessTimeWide() - splash_message_time > 1500000ULL) {
      splash_draw_message(splash_fb);
    }
    return;
  }
  splash_rect(splash_fb, BAR_X + filled, BAR_Y, target - filled, BAR_H, splash_rgb(251, 184, 28));
  filled = target;
  // A new funny message every 1.5 seconds.
  if (sceKernelGetProcessTimeWide() - splash_message_time > 1500000ULL) {
    splash_draw_message(splash_fb);
  }
}

// Called once the game draws its own frames: the early screen can go.
void solarus_vita_splash_free() {
  if (splash_uid >= 0) {
    splash_fb = nullptr;
    sceDisplayWaitVblankStart();
    sceDisplayWaitVblankStart();
    sceKernelFreeMemBlock(splash_uid);
    splash_uid = -1;
  }
}
#endif

namespace Solarus {

namespace {

/**
 * \brief Prints the usage of the program.
 * \param argc number of command-line arguments
 * \param argv command-line arguments
 */
void print_help(const Arguments& args) {

  std::string binary_name = args.get_program_name();
  if (binary_name.empty()) {
    binary_name = "solarus";
  }
  std::cout << "Usage: " << binary_name << " [options] [quest_path]"
    << std::endl << std::endl
    << "The quest path is the name of a directory that contains either the data"
    << std::endl
    << "directory or the data archive (data.solarus or data.solarus.zip) of the game to run."
    << std::endl
    << "If the quest path is not specified, the default directory will be: '"
    << SOLARUS_DEFAULT_QUEST << "'."
    << std::endl
    << std::endl
    << "Options:"
    << std::endl
    << "  -help                         shows this help message and exits"
    << std::endl
    << "  -no-audio                     disables sounds and musics"
    << std::endl
    << "  -no-video                     disables displaying"
    << std::endl
    << "  -quest-size=<width>x<height>  sets the size of the drawing area (if compatible with the quest)"
    << std::endl
    << "  -lua-console=yes|no           accepts standard input lines as Lua commands (default no)"
    << std::endl
    << "  -turbo=yes|no                 runs as fast as possible rather than simulating real time (default no)"
    << std::endl
    << "  -lag=X                        slows down each frame of X milliseconds to simulate slower systems for debugging (default 0)"
    << std::endl
    << "  -cursor-visible=yes|no        sets the mouse cursor visibility on start (default leave unchanged)"
    << std::endl
    << "  -fullscreen=yes|no            sets fullscreen mode on start (default leave unchanged)"
    << std::endl
    << "  -suspend-unfocused=yes|no     suspends the simulation when the application window is not focused (default yes)"
    << std::endl
    << "  -perf-sound-play=yes|no       enables performance reporting of sound playing (default no)"
    << std::endl
    << "  -perf-video-render=yes|no     enables performance reporting of video rendering, i.e. FPS (default no)"
    << std::endl
    << "  -joypad-deadzone=<value>      sets the joypad axis deadzone between 0-32767 (default 10000)"
    << std::endl
    << "  -quit-combo=<b1>+<b2>+...     enables joypad buttons combo for quitting (default disabled)"
    << std::endl
    << "  -s=<code>                     execute some Lua code before the main.lua script of the quest."
    << std::endl
    << "  -force-software-rendering     force the engine to use SDL software rendering. Disabling opengl."
    << std::endl;
}

}  // Anonymous namespace.

}  // namespace Solarus.

/**
 * \brief Usual entry point of the program.
 *
 * Refer to print_help() above for more information.
 *
 * \param argc Number of command-line arguments.
 * \param argv Command-line arguments.
 */
int main(int argc, char** argv) {

  using namespace Solarus;
#ifdef __vita__
  // VITA_CLOCKS: run the CPU, bus and GPU at their maximum speed.
  scePowerSetArmClockFrequency(444);
  scePowerSetBusClockFrequency(222);
  scePowerSetGpuClockFrequency(222);
  scePowerSetGpuXbarClockFrequency(166);
  splash_show();
  {
    FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
    if (dbg) {
      fprintf(dbg, "SOLARUS_DEBUG: [%u ms] DEBUT DU PROGRAMME\n",
          static_cast<unsigned>(sceKernelGetProcessTimeWide() / 1000));
      fclose(dbg);
    }
  }
#endif

  Debug::set_abort_on_die(true);  // Better for debugging (get a callstack).

  // Store the command-line arguments.
  const Arguments args(argc, argv);

  // Check the -help option.
  if (args.has_argument("-help")) {
    // Print a help message.
    print_help(args);
  }
  else {
    // Run the main loop.
    { MainLoop main_loop(args); main_loop.run();
#ifdef __vita__
      sceKernelExitProcess(0);
#endif
    }
  }

  return 0;
}

#ifdef __vita__
extern "C" {
unsigned int _newlib_heap_size_user = 256 * 1024 * 1024;
}
#endif

#ifdef __vita__
// VITA_RT_CACHE: the Vita can only have a limited number of GPU render
// targets (SCE_GXM_ERROR_OUT_OF_RENDER_TARGETS). SDL creates one per target
// texture; we share them between textures of the same size.
#include <psp2/gxm.h>
#include <vector>
#include <cstdio>
extern "C" {
int __real_sceGxmCreateRenderTarget(const SceGxmRenderTargetParams* params, SceGxmRenderTarget** render_target);
int __real_sceGxmDestroyRenderTarget(SceGxmRenderTarget* render_target);
}
namespace {
struct VitaRtEntry {
  unsigned w, h, flags, msaa, spf;
  SceGxmRenderTarget* rt;
  int refs;
};
std::vector<VitaRtEntry>& vita_rt_cache() {
  static std::vector<VitaRtEntry> cache;
  return cache;
}
}
extern "C" int __wrap_sceGxmCreateRenderTarget(const SceGxmRenderTargetParams* p, SceGxmRenderTarget** out) {
  if (p == nullptr || out == nullptr || p->driverMemBlock != -1) {
    return __real_sceGxmCreateRenderTarget(p, out);
  }
  std::vector<VitaRtEntry>& cache = vita_rt_cache();
  for (VitaRtEntry& e : cache) {
    if (e.w == p->width && e.h == p->height && e.flags == p->flags &&
        e.msaa == p->multisampleMode && e.spf == p->scenesPerFrame) {
      ++e.refs;
      *out = e.rt;
      return 0;
    }
  }
  int err = __real_sceGxmCreateRenderTarget(p, out);
  if (err < 0) {
    // Free the cached render targets nobody uses anymore, then retry.
    for (auto it = cache.begin(); it != cache.end();) {
      if (it->refs == 0) {
        __real_sceGxmDestroyRenderTarget(it->rt);
        it = cache.erase(it);
      }
      else {
        ++it;
      }
    }
    err = __real_sceGxmCreateRenderTarget(p, out);
  }
  if (err >= 0) {
    cache.push_back({ p->width, p->height, p->flags, p->multisampleMode, p->scenesPerFrame, *out, 1 });
  }
  FILE* f = (err >= 0) ? nullptr : fopen("ux0:data/solarus_debug.log", "a");
  if (f) {
    fprintf(f, "RT new %ux%u %s (tailles en cache: %u)\n", (unsigned) p->width, (unsigned) p->height,
        err >= 0 ? "ok" : "FAILED", (unsigned) cache.size());
    fclose(f);
  }
  return err;
}
extern "C" int __wrap_sceGxmDestroyRenderTarget(SceGxmRenderTarget* rt) {
  for (VitaRtEntry& e : vita_rt_cache()) {
    if (e.rt == rt) {
      if (e.refs > 0) {
        --e.refs;
      }
      return 0;
    }
  }
  return __real_sceGxmDestroyRenderTarget(rt);
}
#endif
