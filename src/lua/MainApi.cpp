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
#ifdef __vita__
#include <curl/curl.h>
#include <psp2/appmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/apputil.h>
#include <psp2/system_param.h>
#include <SDL.h>
#include <SDL_image.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "solarus/graphics/Surface.h"
#include "solarus/graphics/Video.h"
#endif
#include "solarus/core/CurrentQuest.h"
#include "solarus/core/Game.h"
#include "solarus/core/Savegame.h"
#include "solarus/core/Geometry.h"
#include "solarus/core/MainLoop.h"
#include "solarus/core/QuestFiles.h"
#include "solarus/core/QuestDatabase.h"
#include "solarus/core/QuestProperties.h"
#include "solarus/core/Settings.h"
#include "solarus/core/System.h"
#include "solarus/lua/LuaBind.h"
#include "solarus/lua/LuaContext.h"
#include "solarus/lua/LuaTools.h"
#include <lua.hpp>

namespace Solarus {

/**
 * Name of the Lua table representing the main module of Solarus.
 */
const std::string LuaContext::main_module_name = "sol.main";

template<typename T>
auto empty_to_opt(const T& val) -> std::optional<T> {
  if(val.empty()) {
    return std::nullopt;
  }
  return val;
}



/**
 * \brief Implementation of sol.main.get_solarus_version().
 * \return Solarus version string
 */
static const std::string& get_solarus_version() {
  static const std::string version(SOLARUS_VERSION);
  return version;
}

/**
 * \brief Implementation of sol.main.get_quest_format().
 * \return Quest format string
 */
static std::optional<std::string> get_quest_format() {
  return empty_to_opt(CurrentQuest::get_properties().get_solarus_version());
}

/**
 * @brief Implementation of sol.main.load_file()
 * @param ctx the lua context
 * @param filename name of the file to load
 * @return 1 OnStack chunk value
 */
static LuaBind::OnStack load_file(LuaContext& ctx, const std::string& filename) {
  if(!ctx.load_file(filename)) {
    lua_pushnil(ctx.get_internal_state());
  }
  return {1};
}

/**
 * @brief Implementation of sol.main.do_file()
 * @param ctx the lua context
 * @param filename name of the file to execute
 * @return 0 OnStack values
 */
static void do_file(LuaContext& ctx, const std::string& filename) {
  ctx.do_file(filename);
}

/**
 * @brief Implementation of sol.main.reset()
 * @param ctx the lua context
 */
static void reset(LuaContext& ctx) {
  ctx.get_main_loop().set_resetting();
}

/**
 * @brief Implementation of sol.main.exit()
 * @param ctx the lua context
 */
static void exit(LuaContext& ctx) {
  ctx.get_main_loop().set_exiting();
}

/**
 * @brief Implementation of sol.main.get_quest_write_dir()
 * @return [string]
 */
static std::optional<std::string> get_quest_write_dir() {
  return empty_to_opt(QuestFiles::get_quest_write_dir());
}

/**
 * @brief Implementation of sol.main.set_quest_write_dir(dir)
 * @param [string] dir
 */
static void set_quest_write_dir(const std::optional<std::string>& dir) {
  QuestFiles::set_quest_write_dir(dir.value_or(""));
}

/**
 * @brief Implementation of sol.main.load_settings(file_name)
 * @param ctx the lua context
 * @param ofile_name [string] settings file name name
 * @return bool : wether the loading was sucessful
 */
static bool load_settings(LuaContext& ctx, const std::optional<std::string>& ofile_name) {
  auto file_name = ofile_name ? *ofile_name : "settings.dat";
  if (QuestFiles::get_quest_write_dir().empty()) {
    LuaTools::error(ctx.get_internal_state(), "Cannot load settings: no write directory was specified in quest.dat");
  }

  bool success = false;
  if (QuestFiles::data_file_exists(file_name) &&
      !QuestFiles::data_file_is_dir(file_name)) {
    Settings settings;
    success = settings.load(file_name);
    if (success) {
      settings.apply_to_quest();
    }
  }

  return success;
}

/**
 * @brief Implementation of sol.main.save_settings(file_name)
 * @param ctx the lua context
 * @param ofile_name [string] : optional file name
 * @return bool : success
 */
static bool save_settings(LuaContext& ctx, const std::optional<std::string>& ofile_name) {
  auto file_name = ofile_name ? *ofile_name : "settings.dat";
  if (QuestFiles::get_quest_write_dir().empty()) {
    LuaTools::error(ctx.get_internal_state(), "Cannot save settings: no write directory was specified in quest.dat");
  }

  Settings settings;
  settings.set_from_quest();
  return settings.save(file_name);
}

/**
 * @brief Implementation of sol.main.get_distance(x1, y1, x2, y2)
 * @param x1
 * @param y1
 * @param x2
 * @param y2
 * @return integer euclidian distance between pairs of coords
 */
static int get_distance(int x1, int y1, int x2, int y2) {
  return Geometry::get_distance(x1, y1, x2, y2);
}

/**
 * @brief Implementation of sol.main.get_angle(x1, y1, x2, y2)
 * @param x1
 * @param y1
 * @param x2
 * @param y2
 * @return angle in radian between x axis and vector from p1 to p2
 */
static double get_angle(int x1, int y1, int x2, int y2) {
  return Geometry::get_angle(x1, y1, x2, y2);
}

/**
 * @brief Implementation of sol.main.get_type(obj)
 * @param ctx the lua context
 * @return string : typename
 */
static std::string get_type(lua_State* L) {
  luaL_checkany(L, 1);
  return LuaTools::get_type_name(L, 1);
}


/**
 * @brief Implementation of sol.main.get_metatable(type_name)
 * @param ctx the lua context
 * @param type_name a name
 * @return [table] : type's metatable if any
 */
static LuaBind::OnStack get_metatable(LuaContext& ctx, const std::string& type_name) {
  auto l = ctx.get_internal_state();
  luaL_getmetatable(l, (std::string("sol.") + type_name).c_str());
  return {1};
}

/**
 * @brief Implementation of sol.main.get_quest_version()
 * @return [string] : optional quest version string
 */
static std::optional<std::string> get_quest_version() {
  return empty_to_opt(CurrentQuest::get_properties().get_quest_version());
}

/**
 * @brief Implementation of sol.main.get_resource_ids(type)
 * @param type string : a resource type tag
 * @return array : list of resource ids
 */
static std::vector<std::string> get_resource_ids(ResourceType type) {
  const auto& elements = CurrentQuest::get_database().get_resource_elements(type);
  auto arr = std::vector<std::string>();
  arr.reserve(elements.size());
  for(const auto& [k,v] : elements) {
    arr.push_back(k);
  }
  return arr;
}

/**
 * @brief Implementation of sol.main.get_resource_description(type, id)
 * @param ctx the lua context
 * @param type resource type name
 * @param id
 * @return [string] : resource description
 */
static std::optional<std::string> get_resource_description(LuaContext& ctx, ResourceType type, const std::string& id) {
  const QuestDatabase& database = CurrentQuest::get_database();
  if (!database.resource_exists(type, id)) {
    LuaTools::arg_error(ctx.get_internal_state(), 2, "No such resource element: '" + id + "'");
  }

  return empty_to_opt(database.get_description(type, id));
}

/**
 * @brief Implementation of sol.main.add_resource(type, id, [description])
 * @param ctx the lua context
 * @param type resource type name
 * @param id resource id
 * @param description optional description
 */
static void add_resource(LuaContext& ctx, ResourceType type, const std::string& id, const std::optional<std::string>& description) {
  QuestDatabase& database = CurrentQuest::get_database();
  if (database.resource_exists(type, id)) {
    LuaTools::arg_error(ctx.get_internal_state(), 2, "Resource element already exists: '" + id + "'");
  }

  database.add(type, id, description ? *description : "");
}

/**
 * @brief Implementation of sol.main.remove_resource(type, id)
 * @param ctx the lua context
 * @param type resource type name
 * @param id resource id
 */
static void remove_resource(LuaContext& ctx, ResourceType type, const std::string& id) {
  QuestDatabase& database = CurrentQuest::get_database();
  if (!database.resource_exists(type, id)) {
    LuaTools::arg_error(ctx.get_internal_state(), 2, "No such resource element: '" + id + "'");
  }

  database.remove(type, id);
}

/**
 * @brief Implementation of sol.main.get_game()
 * @param ctx the lua context
 * @return [game] : the current game if any
 */
static Savegame* get_game(LuaContext& ctx) {
  Game* game = ctx.get_main_loop().get_game();
  if (game != nullptr && game->has_savegame()) {
    return &game->get_savegame();
  }
  return nullptr;
}

/**
 * @brief Implementation of sol.main.rawget(t, k)
 * @param l the lua state
 * @return
 */
static LuaBind::OnStack rawget(lua_State* l) {
  switch (lua_type(l, 1)) {
  case LUA_TUSERDATA:
    return {LuaContext::userdata_rawget_as_table(l)};
  case LUA_TTABLE:
    if (2 < LuaTools::check_mintop(l, 2)) {
      lua_settop(l, 2);
    }
    lua_rawget(l, 1);
    return {1};
  default:
    LuaTools::type_error(l, 1, "table or userdata");
  }
}

/**
 * @brief Implementation of sol.main.rawset(t, k, v)
 * @param l the lua state
 * @return
 */
static LuaBind::OnStack rawset(lua_State* l) {
  switch (lua_type(l, 1)) {
  case LUA_TUSERDATA:
    LuaContext::userdata_meta_newindex_as_table(l);
    // Take advantage of the fact newindex leaves the arguments in place.
    lua_settop(l, 1);
    return {1};
  case LUA_TTABLE:
    if (3 < LuaTools::check_mintop(l, 3)) {
      lua_settop(l, 3);
    }
    lua_rawset(l, 1);
    return {1};
  default:
    LuaTools::type_error(l, 1, "table or userdata");
  }
}

#ifdef __vita__
/**
 * Language of the console: "fr", "en", "es", "de", "it", "nl", "pt", "ru",
 * "ja", "ko", "zh", "pl" ("en" for the others).
 * Also used by MainLoop to choose the language of the games.
 */
std::string vita_console_language() {
  static std::string code;
  if (code.empty()) {
    SceAppUtilInitParam init_param;
    SceAppUtilBootParam boot_param;
    memset(&init_param, 0, sizeof(init_param));
    memset(&boot_param, 0, sizeof(boot_param));
    sceAppUtilInit(&init_param, &boot_param);  // May already be done: ignore errors.
    int lang = SCE_SYSTEM_PARAM_LANG_ENGLISH_US;
    sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, &lang);
    switch (lang) {
      case SCE_SYSTEM_PARAM_LANG_JAPANESE: code = "ja"; break;
      case SCE_SYSTEM_PARAM_LANG_FRENCH: code = "fr"; break;
      case SCE_SYSTEM_PARAM_LANG_SPANISH: code = "es"; break;
      case SCE_SYSTEM_PARAM_LANG_GERMAN: code = "de"; break;
      case SCE_SYSTEM_PARAM_LANG_ITALIAN: code = "it"; break;
      case SCE_SYSTEM_PARAM_LANG_DUTCH: code = "nl"; break;
      case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT:
      case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR: code = "pt"; break;
      case SCE_SYSTEM_PARAM_LANG_RUSSIAN: code = "ru"; break;
      case SCE_SYSTEM_PARAM_LANG_KOREAN: code = "ko"; break;
      case SCE_SYSTEM_PARAM_LANG_CHINESE_T:
      case SCE_SYSTEM_PARAM_LANG_CHINESE_S: code = "zh"; break;
      case SCE_SYSTEM_PARAM_LANG_POLISH: code = "pl"; break;
      default: code = "en"; break;
    }
  }
  return code;
}

namespace {

// ----- Vita launcher support: download (non-blocking), launch, file access.

char vita_net_mem[1024 * 1024];
bool vita_net_ready = false;

CURLM* dl_multi = nullptr;
CURL* dl_easy = nullptr;
FILE* dl_file = nullptr;
std::string dl_state = "idle";
std::string dl_error;
std::string dl_dest;
std::string dl_part;
curl_off_t dl_bytes = 0;
curl_off_t dl_total = 0;
char dl_errbuf[CURL_ERROR_SIZE];

size_t dl_write(void* p, size_t size, size_t n, void* user) {
  return fwrite(p, size, n, static_cast<FILE*>(user));
}

void vita_net_init() {
  if (vita_net_ready) {
    return;
  }
  sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
  SceNetInitParam init_param;
  init_param.memory = vita_net_mem;
  init_param.size = sizeof(vita_net_mem);
  init_param.flags = 0;
  sceNetInit(&init_param);
  sceNetCtlInit();
  curl_global_init(CURL_GLOBAL_DEFAULT);
  vita_net_ready = true;
}

void dl_cleanup() {
  if (dl_easy != nullptr && dl_multi != nullptr) {
    curl_multi_remove_handle(dl_multi, dl_easy);
  }
  if (dl_easy != nullptr) {
    curl_easy_cleanup(dl_easy);
    dl_easy = nullptr;
  }
  if (dl_multi != nullptr) {
    curl_multi_cleanup(dl_multi);
    dl_multi = nullptr;
  }
  if (dl_file != nullptr) {
    fclose(dl_file);
    dl_file = nullptr;
  }
}

void vita_write_text(const char* path, const std::string& text) {
  FILE* f = fopen(path, "w");
  if (f != nullptr) {
    fputs(text.c_str(), f);
    fclose(f);
  }
}

/** sol.main.vita_download_start(url, dest_path) -> boolean */
int vita_download_start(lua_State* l) {
  const char* url = luaL_checkstring(l, 1);
  const char* dest = luaL_checkstring(l, 2);
  dl_cleanup();
  vita_net_init();
  dl_dest = dest;
  dl_part = dl_dest + ".part";
  dl_bytes = 0;
  dl_total = 0;
  dl_error.clear();
  dl_errbuf[0] = '\0';
  dl_file = fopen(dl_part.c_str(), "wb");
  if (dl_file == nullptr) {
    dl_state = "error";
    dl_error = "cannot write " + dl_part;
    lua_pushboolean(l, 0);
    return 1;
  }
  dl_multi = curl_multi_init();
  dl_easy = curl_easy_init();
  curl_easy_setopt(dl_easy, CURLOPT_URL, url);
  curl_easy_setopt(dl_easy, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(dl_easy, CURLOPT_MAXREDIRS, 10L);
  curl_easy_setopt(dl_easy, CURLOPT_WRITEFUNCTION, dl_write);
  curl_easy_setopt(dl_easy, CURLOPT_WRITEDATA, dl_file);
  curl_easy_setopt(dl_easy, CURLOPT_USERAGENT, "SolarusVita/0.1");
  curl_easy_setopt(dl_easy, CURLOPT_CONNECTTIMEOUT, 20L);
  curl_easy_setopt(dl_easy, CURLOPT_ERRORBUFFER, dl_errbuf);
  // TODO: ship a CA bundle and enable certificate verification.
  curl_easy_setopt(dl_easy, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(dl_easy, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_multi_add_handle(dl_multi, dl_easy);
  dl_state = "running";
  lua_pushboolean(l, 1);
  return 1;
}

/** sol.main.vita_download_poll() -> { state, bytes, total, error } */
int vita_download_poll(lua_State* l) {
  if (dl_state == "running" && dl_multi != nullptr) {
    int running = 0;
    curl_multi_perform(dl_multi, &running);
    curl_easy_getinfo(dl_easy, CURLINFO_SIZE_DOWNLOAD_T, &dl_bytes);
    curl_easy_getinfo(dl_easy, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &dl_total);
    if (running == 0) {
      bool ok = false;
      int msgs = 0;
      CURLMsg* msg;
      while ((msg = curl_multi_info_read(dl_multi, &msgs)) != nullptr) {
        if (msg->msg == CURLMSG_DONE) {
          long code = 0;
          curl_easy_getinfo(dl_easy, CURLINFO_RESPONSE_CODE, &code);
          if (msg->data.result == CURLE_OK && code == 200) {
            ok = true;
          }
          else if (msg->data.result != CURLE_OK) {
            dl_error = curl_easy_strerror(msg->data.result);
          }
          else {
            dl_error = "HTTP " + std::to_string(code);
          }
        }
      }
      dl_cleanup();
      if (ok) {
        sceIoRemove(dl_dest.c_str());
        if (sceIoRename(dl_part.c_str(), dl_dest.c_str()) >= 0) {
          dl_state = "done";
        }
        else {
          dl_state = "error";
          dl_error = "cannot rename downloaded file";
        }
      }
      else {
        sceIoRemove(dl_part.c_str());
        dl_state = "error";
        if (dl_error.empty()) {
          dl_error = "download failed";
        }
      }
    }
  }
  lua_newtable(l);
  lua_pushstring(l, dl_state.c_str());
  lua_setfield(l, -2, "state");
  lua_pushnumber(l, static_cast<lua_Number>(dl_bytes));
  lua_setfield(l, -2, "bytes");
  lua_pushnumber(l, static_cast<lua_Number>(dl_total));
  lua_setfield(l, -2, "total");
  lua_pushstring(l, dl_error.c_str());
  lua_setfield(l, -2, "error");
  return 1;
}

/** sol.main.vita_download_cancel() */
int vita_download_cancel(lua_State* /* l */) {
  dl_cleanup();
  if (!dl_part.empty()) {
    sceIoRemove(dl_part.c_str());
  }
  dl_state = "idle";
  return 0;
}

/** sol.main.vita_file_size(path) -> number or nil */
int vita_file_size(lua_State* l) {
  const char* path = luaL_checkstring(l, 1);
  FILE* f = fopen(path, "rb");
  if (f == nullptr) {
    lua_pushnil(l);
    return 1;
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  lua_pushnumber(l, static_cast<lua_Number>(size));
  return 1;
}

/** sol.main.vita_read_file(path) -> string, or nil + message */
int vita_read_file(lua_State* l) {
  const char* path = luaL_checkstring(l, 1);
  FILE* f = fopen(path, "rb");
  if (f == nullptr) {
    lua_pushnil(l);
    lua_pushstring(l, "cannot open file");
    return 2;
  }
  std::string content;
  char buffer[4096];
  size_t n;
  while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
    content.append(buffer, n);
  }
  fclose(f);
  lua_pushlstring(l, content.data(), content.size());
  return 1;
}

/** sol.main.vita_launch(quest_path) -> boolean (only returns on failure) */
int vita_launch(lua_State* l) {
  const char* quest = luaL_checkstring(l, 1);
  vita_write_text("ux0:data/Solarus/quest.txt", std::string(quest) + "\n");
  vita_write_text("ux0:data/Solarus/mode.txt", "play\n");
  int result = sceAppMgrLoadExec("app0:eboot.bin", nullptr, nullptr);
  lua_pushboolean(l, result >= 0);
  return 1;
}

/** sol.main.vita_back_to_launcher() -> boolean (only returns on failure) */
int vita_back_to_launcher(lua_State* l) {
  sceIoRemove("ux0:data/Solarus/mode.txt");
  int result = sceAppMgrLoadExec("app0:eboot.bin", nullptr, nullptr);
  lua_pushboolean(l, result >= 0);
  return 1;
}

/**
 * sol.main.vita_make_thumbnail(src_path, dst_path, width, height) -> boolean
 * Loads an image (PNG or JPG), crops it to the width/height ratio, shrinks it
 * with an averaging filter and saves it as a PNG.
 */
int vita_make_thumbnail(lua_State* l) {
  const char* src_path = luaL_checkstring(l, 1);
  const char* dst_path = luaL_checkstring(l, 2);
  const int w = static_cast<int>(luaL_checkinteger(l, 3));
  const int h = static_cast<int>(luaL_checkinteger(l, 4));
  SDL_Surface* loaded = IMG_Load(src_path);
  if (loaded == nullptr || w <= 0 || h <= 0) {
    if (loaded != nullptr) {
      SDL_FreeSurface(loaded);
    }
    lua_pushboolean(l, false);
    return 1;
  }
  SDL_Surface* src = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(loaded);
  SDL_Surface* dst = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
  if (src == nullptr || dst == nullptr) {
    if (src != nullptr) SDL_FreeSurface(src);
    if (dst != nullptr) SDL_FreeSurface(dst);
    lua_pushboolean(l, false);
    return 1;
  }
  // Crop the source to the destination ratio (centered).
  int cw = src->w;
  int ch = src->w * h / w;
  if (ch > src->h) {
    ch = src->h;
    cw = src->h * w / h;
  }
  const int cx = (src->w - cw) / 2;
  const int cy = (src->h - ch) / 2;
  SDL_LockSurface(src);
  SDL_LockSurface(dst);
  const unsigned char* sp = static_cast<const unsigned char*>(src->pixels);
  unsigned char* dp = static_cast<unsigned char*>(dst->pixels);
  for (int y = 0; y < h; ++y) {
    const int y0 = cy + y * ch / h;
    int y1 = cy + (y + 1) * ch / h;
    if (y1 <= y0) y1 = y0 + 1;
    for (int x = 0; x < w; ++x) {
      const int x0 = cx + x * cw / w;
      int x1 = cx + (x + 1) * cw / w;
      if (x1 <= x0) x1 = x0 + 1;
      unsigned int sum[4] = { 0, 0, 0, 0 };
      unsigned int count = 0;
      for (int yy = y0; yy < y1; ++yy) {
        const unsigned char* row = sp + yy * src->pitch;
        for (int xx = x0; xx < x1; ++xx) {
          const unsigned char* px = row + xx * 4;
          sum[0] += px[0]; sum[1] += px[1]; sum[2] += px[2]; sum[3] += px[3];
          ++count;
        }
      }
      unsigned char* out = dp + y * dst->pitch + x * 4;
      for (int c = 0; c < 4; ++c) {
        out[c] = static_cast<unsigned char>(sum[c] / count);
      }
    }
  }
  SDL_UnlockSurface(dst);
  SDL_UnlockSurface(src);
  const bool ok = IMG_SavePNG(dst, dst_path) == 0;
  SDL_FreeSurface(src);
  SDL_FreeSurface(dst);
  lua_pushboolean(l, ok);
  return 1;
}

/** sol.main.vita_load_image(path) -> surface, or nil (any path, PNG or JPG) */
int vita_load_image(lua_State* l) {
  const char* path = luaL_checkstring(l, 1);
  SDL_Surface* loaded = IMG_Load(path);
  if (loaded == nullptr) {
    lua_pushnil(l);
    return 1;
  }
  SDL_Surface* converted = SDL_ConvertSurface(loaded, Video::get_pixel_format(), 0);
  SDL_FreeSurface(loaded);
  if (converted == nullptr) {
    lua_pushnil(l);
    return 1;
  }
  SurfacePtr surface = Surface::create(SDL_Surface_UniquePtr(converted), false);
  LuaContext::push_surface(l, *surface);
  return 1;
}

/** sol.main.vita_remove_file(path) -> boolean */
int vita_remove_file(lua_State* l) {
  const char* path = luaL_checkstring(l, 1);
  lua_pushboolean(l, sceIoRemove(path) >= 0);
  return 1;
}

/** sol.main.vita_make_dir(path) -> boolean */
int vita_make_dir(lua_State* l) {
  const char* path = luaL_checkstring(l, 1);
  const int r = sceIoMkdir(path, 0777);
  lua_pushboolean(l, r >= 0 || r == static_cast<int>(0x80010011));  // already exists
  return 1;
}

/** sol.main.vita_list_dir(path) -> array of file names, or nil */
int vita_list_dir(lua_State* l) {
  const char* path = luaL_checkstring(l, 1);
  const SceUID d = sceIoDopen(path);
  if (d < 0) {
    lua_pushnil(l);
    return 1;
  }
  lua_newtable(l);
  int i = 1;
  SceIoDirent entry;
  while (sceIoDread(d, &entry) > 0) {
    lua_pushstring(l, entry.d_name);
    lua_rawseti(l, -2, i++);
  }
  sceIoDclose(d);
  return 1;
}

/** Deletes a folder and everything in it. */
bool vita_remove_tree(const std::string& path) {
  const SceUID d = sceIoDopen(path.c_str());
  if (d < 0) {
    return false;
  }
  SceIoDirent entry;
  std::vector<std::string> dirs;
  while (sceIoDread(d, &entry) > 0) {
    const std::string name = entry.d_name;
    if (name == "." || name == "..") {
      continue;
    }
    const std::string child = path + "/" + name;
    if (SCE_S_ISDIR(entry.d_stat.st_mode)) {
      dirs.push_back(child);
    } else {
      sceIoRemove(child.c_str());
    }
  }
  sceIoDclose(d);
  for (const std::string& dir : dirs) {
    vita_remove_tree(dir);
  }
  return sceIoRmdir(path.c_str()) >= 0;
}

/** sol.main.vita_remove_dir(path) -> boolean (deletes the folder and its content) */
int vita_remove_dir(lua_State* l) {
  const std::string path = luaL_checkstring(l, 1);
  lua_pushboolean(l, vita_remove_tree(path));
  return 1;
}

/** sol.main.vita_get_saves_dir() -> folder that contains the save folder of each game */
int vita_get_saves_dir(lua_State* l) {
  std::string base = QuestFiles::get_base_write_dir();
  while (!base.empty() && base.back() == '/') {
    base.pop_back();
  }
  const std::string dir = base + "/" + QuestFiles::get_solarus_write_dir();
  lua_pushstring(l, dir.c_str());
  return 1;
}

/**
 * sol.main.vita_get_language() -> "fr", "en", "es", "de", "it", "nl", "pt",
 * "ru", "ja", "ko", "zh", "pl", ... : language of the console.
 */
int vita_get_language(lua_State* l) {
  lua_pushstring(l, vita_console_language().c_str());
  return 1;
}

}  // Anonymous namespace.
#endif

/**
 * \brief Initializes the main features provided to Lua.
 */
void LuaContext::register_main_module() {

  std::vector<luaL_Reg> functions = {
      { "get_solarus_version", LUA_TO_C_BIND(get_solarus_version) },
      { "get_quest_format", LUA_TO_C_BIND(get_quest_format) },
      { "load_file", LUA_TO_C_BIND(Solarus::load_file) },
      { "do_file", LUA_TO_C_BIND(Solarus::do_file) },
      { "reset", LUA_TO_C_BIND(reset) },
      { "exit", LUA_TO_C_BIND(Solarus::exit) },
      { "get_elapsed_time", LUA_TO_C_BIND(System::now_ms) },
      { "get_quest_write_dir", LUA_TO_C_BIND(get_quest_write_dir) },
      { "set_quest_write_dir", LUA_TO_C_BIND(set_quest_write_dir) },
      { "load_settings", LUA_TO_C_BIND(load_settings) },
      { "save_settings", LUA_TO_C_BIND(save_settings) },
      { "get_distance", LUA_TO_C_BIND(get_distance) },
      { "get_angle", LUA_TO_C_BIND(get_angle) },
      { "get_type", LUA_TO_C_BIND(get_type) },
      { "get_metatable", LUA_TO_C_BIND(get_metatable) },
      { "get_os", LUA_TO_C_BIND(System::get_os) }
#ifdef __vita__
      ,{ "vita_download_start", vita_download_start }
      ,{ "vita_download_poll", vita_download_poll }
      ,{ "vita_download_cancel", vita_download_cancel }
      ,{ "vita_file_size", vita_file_size }
      ,{ "vita_read_file", vita_read_file }
      ,{ "vita_launch", vita_launch }
      ,{ "vita_back_to_launcher", vita_back_to_launcher }
      ,{ "vita_make_thumbnail", vita_make_thumbnail }
      ,{ "vita_load_image", vita_load_image }
      ,{ "vita_remove_file", vita_remove_file }
      ,{ "vita_make_dir", vita_make_dir }
      ,{ "vita_list_dir", vita_list_dir }
      ,{ "vita_remove_dir", vita_remove_dir }
      ,{ "vita_get_saves_dir", vita_get_saves_dir }
      ,{ "vita_get_language", vita_get_language }
#endif
  };
  if (CurrentQuest::is_format_at_least({ 1, 6 })) {
    functions.insert(functions.end(), {
        { "get_quest_version", LUA_TO_C_BIND(get_quest_version) },
        { "get_resource_ids", LUA_TO_C_BIND(get_resource_ids) },
        { "resource_exists", LUA_TO_C_BIND(CurrentQuest::resource_exists)},
        { "get_resource_description", LUA_TO_C_BIND(get_resource_description) },
        { "add_resource", LUA_TO_C_BIND(add_resource) },
        { "remove_resource", LUA_TO_C_BIND(remove_resource) },
        { "get_game", LUA_TO_C_BIND(get_game) },
    });
  }
  if (CurrentQuest::is_format_at_least({ 2, 0 })) {
    functions.insert(functions.end(), {
        { "rawget", LUA_TO_C_BIND(rawget) },
        { "rawset", LUA_TO_C_BIND(rawset) },
    });
  }

  register_functions(main_module_name, functions);

  // Store sol.main in the registry to access it safely
  // from C++ (and also slightly faster).
  // After that, the engine will never rely on the existence of a global
  // value called "sol". The user can therefore do whatever he wants, including
  // renaming the sol global table to something else in the unlikely case where
  // another Lua library called "sol" is required, or if he simply does not
  // like the name "sol".

                                  // ...
  lua_getglobal(current_l, "sol");
                                  // ... sol
  lua_getfield(current_l, -1, "main");
                                  // ... sol main
  lua_setfield(current_l, LUA_REGISTRYINDEX, main_module_name.c_str());
                                  // ... sol
  lua_pop(current_l, 1);
                                  // ...
}

/**
 * \brief Pushes the sol.main table onto the stack.
 * \param l A Lua state.
 */
void LuaContext::push_main(lua_State* l) {

  lua_getfield(l, LUA_REGISTRYINDEX, main_module_name.c_str());
}

/**
 * \brief Returns whether a value is the sol.main table.
 * \param l A Lua context.
 * \param index An index in the stack.
 * \return \c true if the value at this index is sol.main.
 */
bool LuaContext::is_main(lua_State* l, int index) {
  push_main(l);
  bool result = lua_equal(l, index, -1);
  lua_pop(l, 1);
  return result;
}

/**
 * \brief Calls sol.main.on_started() if it exists.
 *
 * This function is called when the engine requests Lua to show an
 * initial screen, i.e. at the beginning of the program
 * or when the program is reset.
 */
void LuaContext::main_on_started() {

  push_main(current_l);
  on_started();
  lua_pop(current_l, 1);
}

/**
 * \brief Calls sol.main.on_finished() if it exists.
 *
 * This function is called when the program is reset or stopped.
 */
void LuaContext::main_on_finished() {

  push_main(current_l);
  on_finished();
  remove_timers(-1);  // Stop timers associated to sol.main.
  remove_menus(-1);  // Stop menus associated to sol.main.
  lua_pop(current_l, 1);
}

/**
 * \brief Calls sol.main.on_update() if it exists.
 *
 * This function is called at each cycle by the main loop.
 */
void LuaContext::main_on_update() {
  current_l = main_l; //Always execute main on the main thread
  push_main(current_l);
  on_update();
  menus_on_update(-1);
  lua_pop(current_l, 1);
}

/**
 * \brief Calls sol.main.on_draw() if it exists.
 * \param dst_surface The destination surface.
 */
void LuaContext::main_on_draw(const SurfacePtr& dst_surface) {

  push_main(current_l);
  on_draw(dst_surface);
  menus_on_draw(-1, dst_surface);
  lua_pop(current_l, 1);
}

/**
 * \brief Notifies Lua that an input event has just occurred.
 *
 * The appropriate callback in sol.main is triggered if it exists.
 *
 * \param event The input event to handle.
 * \return \c true if the event was handled and should stop being propagated.
 */
bool LuaContext::main_on_input(const InputEvent& event) {

  push_main(current_l);
  bool handled = on_input(event);
  if (!handled) {
    handled = menus_on_input(-1, event);
  }
  lua_pop(current_l, 1);
  return handled;
}

/**
 * \brief Notifies Lua that an input event has just occurred.
 *
 * The appropriate callback in sol.main is triggered if it exists.
 *
 * \param event The input event to handle.
 * \return \c true if the event was handled and should stop being propagated.
 */
bool LuaContext::main_on_control(const ControlEvent& event) {

  push_main(current_l);
  bool handled = on_command(event);
  if (!handled) {
    handled = menus_on_command(-1, event);
  }
  lua_pop(current_l, 1);
  return handled;
}

}

