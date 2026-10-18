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
#include <variant>
#include <string>
#include <psp2/kernel/clib.h>
#include <cstdio>
#endif
#ifdef __vita__
#include <psp2/appmgr.h>
#include <psp2/io/stat.h>
#include <set>
#include <vector>
#include <cstring>
#endif
#include "solarus/audio/MusicSystem.h"
#include "solarus/core/Arguments.h"
#include "solarus/core/CurrentQuest.h"
#include "solarus/core/Debug.h"
#include "solarus/core/Game.h"
#include "solarus/core/Logger.h"
#include "solarus/core/MainLoop.h"
#include "solarus/core/QuestFiles.h"
#include "solarus/core/QuestDatabase.h"
#include "solarus/core/QuestProperties.h"
#include "solarus/core/Savegame.h"
#include "solarus/core/Settings.h"
#include "solarus/core/String.h"
#include "solarus/core/System.h"
#include "solarus/entities/TilePattern.h"
#include "solarus/graphics/Color.h"
#include "solarus/graphics/Surface.h"
#include "solarus/graphics/Video.h"
#include "solarus/graphics/quest_icon.h"

#include "solarus/lua/LuaContext.h"
#ifdef __vita__
#include <lua.hpp>
#include <SDL_log.h>
#include <cstdio>
#endif
#include "solarus/lua/LuaTools.h"
#include "solarus/core/Profiler.h"

#include <lua.hpp>
#include <clocale>
#include <sstream>
#include <string>
#include <thread>

#ifdef __vita__
#include <psp2/shellutil.h>
// While the engine loads, the PS button is locked: the Vita shows the
// "forbidden" symbol instead of leaving the app in the middle of a load.
static void vita_lock_ps(bool lock) {
  static bool events_ready = false;
  if (!events_ready) {
    sceShellUtilInitEvents(0);
    events_ready = true;
  }
  if (lock) {
    sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);
  }
  else {
    sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);
  }
}
#endif

#ifdef __vita__
#include <psp2/kernel/processmgr.h>
void solarus_vita_splash_free();  // cli/src/main.cpp
bool solarus_vita_splash_active();
void solarus_vita_splash_progress(float progress);
static int vita_splash_frames = -1;
static unsigned vita_ms() {
  return static_cast<unsigned>(sceKernelGetProcessTimeWide() / 1000);
}
#endif

namespace Solarus {

#ifdef __vita__
extern void (*solarus_vita_gc_hook)();
static LuaContext* vita_gc_ctx = nullptr;
static void vita_sdl_log_cb(void*, int category, SDL_LogPriority priority, const char* message) {
  FILE* f = fopen("ux0:data/solarus_debug.log", "a");
  if (f) { fprintf(f, "SDL[%d/%d] %s\n", category, (int)priority, message); fclose(f); }
}
static void vita_gc_now() {
  if (vita_gc_ctx != nullptr) {
    lua_gc(vita_gc_ctx->get_internal_state(), LUA_GCCOLLECT, 0);
  }
}
#endif

#ifdef __vita__
std::string vita_quest_id;
void vita_first_step_check();
bool vita_menu_is_open();
MainLoop* vita_main_loop = nullptr;
void solarus_vita_layout_start();
void solarus_vita_layout_step();
void solarus_vita_overlay_set(const std::vector<std::string>& lines);
void solarus_vita_overlay_opaque(bool opaque);
extern bool solarus_vita_menu_show_background;  // SDLRenderer.cpp
std::string vita_console_language();  // src/lua/MainApi.cpp

// The game starts directly in the language of the console (if the game has
// it), so most games do not ask for the language any more.
// A language chosen before and saved by the game is still used: the game's
// scripts load their settings after this.
static void vita_apply_console_language() {
  const bool launcher = (vita_quest_id == "launcher" || vita_quest_id.empty());
  if (launcher || !CurrentQuest::get_language().empty()) {
    return;
  }
  const std::string console = vita_console_language();
  const QuestDatabase::ResourceMap& languages =
      CurrentQuest::get_database().get_resource_elements(ResourceType::LANGUAGE);
  std::string chosen;
  for (const auto& kv : languages) {
    const std::string& id = kv.first;
    if (id == console) {
      chosen = id;
      break;
    }
    // "pt" also matches "pt_BR", "zh" matches "zh_CN"...
    if (chosen.empty() && id.compare(0, console.size() + 1, console + "_") == 0) {
      chosen = id;
    }
  }
  FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
  if (dbg) {
    fprintf(dbg, "SOLARUS_DEBUG: console language=%s, game language=%s\n",
        console.c_str(), chosen.empty() ? "(asked by the game)" : chosen.c_str());
    fclose(dbg);
  }
  if (!chosen.empty()) {
    CurrentQuest::set_language(chosen);
  }
}

// Loading screen: drawn once as soon as the video is ready, and kept on
// screen until the first step of the main loop.
static void vita_show_loading() {
  if (::solarus_vita_splash_active()) {
    // The early loading screen (cli/src/main.cpp) stays until the first frame.
    ::solarus_vita_splash_progress(0.75f);
    return;
  }
  std::vector<std::string> lines;
  lines.push_back("SOLARUS");
  lines.push_back("");
  lines.push_back("  CHARGEMENT...");
  if (!vita_quest_id.empty() && vita_quest_id != "launcher") {
    std::string n = vita_quest_id;
    if (n.size() > 30) {
      n = n.substr(0, 30);
    }
    lines.push_back("");
    lines.push_back("~" + n);
  }
  solarus_vita_overlay_opaque(true);
  solarus_vita_overlay_set(lines);
  Video::finish();
}

static void vita_hide_loading() {
  solarus_vita_overlay_opaque(false);
  solarus_vita_overlay_set(std::vector<std::string>());
}
#endif

namespace {

/**
 * \brief Checks that the quest is compatible with the current version of
 * Solarus.
 * \param quest_version Solarus sersion of the quest (major and minor number).
 */
void check_version_compatibility(const std::pair<int, int>& quest_version) {

  const int quest_major_version = quest_version.first;
  const int quest_minor_version = quest_version.second;

  if (quest_version.first == 0) {
    Debug::die("No Solarus version is specified in your quest.dat file");
  }

  // The third digit of the version (patch) is ignored because compatibility
  // is not broken by patches.

  bool compatible = true;
  if (quest_major_version > SOLARUS_MAJOR_VERSION) {
    // The quest is too recent for this engine.
    compatible = false;
  }
  else {
    if (quest_major_version == SOLARUS_MAJOR_VERSION &&
        quest_minor_version > SOLARUS_MINOR_VERSION) {
      // The quest is too recent for this engine.
      compatible = false;
    }
    else {
      // 1.5 quests can be run by Solarus 1.5, 1.6 and 2.0.
      // 1.6 quests can be run by Solarus 1.6 and 2.0.
      if (quest_major_version == 1 &&
          quest_minor_version < 5
      ) {
        compatible = false;
      }
    }
  }

  if (!compatible) {
    std::ostringstream oss;
    oss << "This quest is made for Solarus " << quest_major_version << "."
        << quest_minor_version << ".x but you are running Solarus "
        << SOLARUS_VERSION;
    Debug::die(oss.str());
  }
}

/**
 * \brief Returns path of the quest to run.
 *
 * It may be the path defined as command-line argument,
 * the path defined during the build process, or the current directory
 * if nothing was specified.
 *
 * \param args Command-line arguments in system 8-bit encoding.
 * \return The quest path in UTF-8 encoding.
 */
std::string get_quest_path(const Arguments& args) {

  // If a quest command-line argument was specified, use it instead.
  const std::vector<std::string>& options = args.get_arguments();
  if (!options.empty()
      && !options.back().empty()
      && options.back()[0] != '-') {
    // The last parameter is not an option: it is the quest path.
    return options.back();
  }

#ifdef __vita__
  {
    std::string vita_quest;
    // "play" mode: written by the launcher just before it restarts the app.
    bool play_mode = false;
    FILE* mf = fopen("ux0:data/Solarus/mode.txt", "r");
    if (mf) {
      char mode[16] = { 0 };
      if (fgets(mode, sizeof(mode), mf) &&
          mode[0] == 'p' && mode[1] == 'l' && mode[2] == 'a' && mode[3] == 'y') {
        play_mode = true;
      }
      fclose(mf);
      remove("ux0:data/Solarus/mode.txt");
    }
    // A quest bundled in the VPK (app0:data/) is the launcher: it runs
    // unless the launcher asked to play a downloaded quest.
    FILE* bundled = fopen("app0:data/quest.dat", "rb");
    if (bundled) {
      fclose(bundled);
    }
    FILE* qf = (bundled && !play_mode) ? nullptr : fopen("ux0:data/Solarus/quest.txt", "r");
    if (qf) {
      char line[512];
      if (fgets(line, sizeof(line), qf)) {
        vita_quest = line;
        while (!vita_quest.empty() &&
            (vita_quest.back() == '\n' || vita_quest.back() == '\r' || vita_quest.back() == ' ')) {
          vita_quest.pop_back();
        }
      }
      fclose(qf);
    }
    FILE* dbgf = fopen("ux0:data/solarus_debug.log", "a");
    if (dbgf) {
      fprintf(dbgf, "SOLARUS_DEBUG: play_mode=%d quest.txt path='%s'\n", play_mode ? 1 : 0, vita_quest.c_str());
      fclose(dbgf);
    }
    if (bundled && !play_mode) {
      vita_quest_id = "launcher";
    }
    else {
      std::string n = vita_quest;
      while (!n.empty() && (n.back() == '/' || n.back() == '\\')) {
        n.pop_back();
      }
      size_t slash = n.find_last_of("/\\:");
      if (slash != std::string::npos) {
        n = n.substr(slash + 1);
      }
      size_t dot = n.rfind('.');
      if (dot != std::string::npos && dot > 0) {
        n = n.substr(0, dot);
      }
      vita_quest_id = n.empty() ? std::string("game") : n;
    }
    if (!vita_quest.empty()) {
      return vita_quest;
    }
  }
#endif

  // The default quest path is defined during the build process.
  return SOLARUS_DEFAULT_QUEST;
}

}  // Anonymous namespace.

/**
 * \brief Initializes the game engine.
 * \param args Command-line arguments.
 */
MainLoop::MainLoop(const Arguments& args):
  lua_context(nullptr),
  resource_provider(),
  root_surface(nullptr),
  game(nullptr),
  next_game(nullptr),
  resetting(false),
  exiting(false),
  debug_lag(0),
  lua_console_enabled(false),
  suspend_unfocused(true),
  suspended(false),
  turbo(false),
  lua_commands(),
  lua_commands_mutex(),
  num_lua_commands_pushed(0),
  num_lua_commands_done(0),
  commands_dispatcher(*this)
{
#ifdef __vita__
  vita_lock_ps(true);
#endif

#ifdef SOLARUS_GIT_REVISION
  Logger::info("Solarus " SOLARUS_VERSION " (" SOLARUS_GIT_REVISION ")");
#else
  Logger::info("Solarus " SOLARUS_VERSION);
#endif

  // Main loop settings.
  const std::string lag_arg = args.get_argument_value("-lag");
  if (!lag_arg.empty()) {
    std::istringstream iss(lag_arg);
    iss >> debug_lag;
  }
  const std::string& turbo_arg = args.get_argument_value("-turbo");
  turbo = (turbo_arg == "yes");
  const std::string& suspend_unfocused_arg = args.get_argument_value("-suspend-unfocused");
  suspend_unfocused = suspend_unfocused_arg.empty() || suspend_unfocused_arg == "yes";
  const std::string& lua_console_arg = args.get_argument_value("-lua-console");
  lua_console_enabled = lua_console_arg == "yes";
  lua_script_arg = args.get_argument_value("-s");

  {
    FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
    if (dbg) { fprintf(dbg, "SOLARUS_DEBUG: [%u ms] 1 before open_quest\n", vita_ms()); fclose(dbg); }
  }
  // Try to open the quest.
  const std::string& quest_path = get_quest_path(args);
  Logger::info("Opening quest '" + quest_path + "'");
  if (!QuestFiles::open_quest(args.get_program_name(), quest_path)) {
    Debug::error("No quest was found in the directory '" + quest_path + "'");
    return;
  }
  {
    FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
    if (dbg) { fprintf(dbg, "SOLARUS_DEBUG: [%u ms] 2 after open_quest, before System::initialize\n", vita_ms()); fclose(dbg); }
  }

  // Initialize engine features (audio, video...).
  System::initialize(args, resource_provider);
#ifdef __vita__
  vita_show_loading();
#endif
  {
    FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
    if (dbg) { fprintf(dbg, "SOLARUS_DEBUG: [%u ms] 3 after System::initialize, before CurrentQuest::initialize\n", vita_ms()); fclose(dbg); }
  }

  // Read the quest resource list from data.
  CurrentQuest::initialize();
  {
    FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
    if (dbg) { fprintf(dbg, "SOLARUS_DEBUG: [%u ms] 4 after CurrentQuest::initialize, before load_quest_properties\n", vita_ms()); fclose(dbg); }
  }

  // Read the quest general properties.
  load_quest_properties();
  {
    FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
    if (dbg) { fprintf(dbg, "SOLARUS_DEBUG: [%u ms] 5 after load_quest_properties, before make_root_surface\n", vita_ms()); fclose(dbg); }
  }

  make_root_surface();
  {
    FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
    if (dbg) { fprintf(dbg, "SOLARUS_DEBUG: [%u ms] 6 after make_root_surface, before LuaContext creation\n", vita_ms()); fclose(dbg); }
  }

#ifdef __vita__
  vita_apply_console_language();
#endif

  // Run the Lua world.
  // Do this after the creation of the window, but before showing the window,
  // because Lua might change the video mode initially.
  lua_context = std::unique_ptr<LuaContext>(new LuaContext(*this));
#ifdef __vita__
  vita_gc_ctx = lua_context.get();
  vita_main_loop = this;
  solarus_vita_gc_hook = &vita_gc_now;
  SDL_LogSetOutputFunction(&vita_sdl_log_cb, nullptr);
  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_ERROR);
#endif
  {
    FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
    if (dbg) { fprintf(dbg, "SOLARUS_DEBUG: [%u ms] 7 after LuaContext creation\n", vita_ms()); fclose(dbg); }
#ifdef __vita__
    ::solarus_vita_splash_progress(0.8f);
#endif
  }

  if(Video::get_renderer().needs_window_workaround()) {
    Video::show_window();
    lua_context->initialize(lua_script_arg);
    Video::hide_window();
  } else {
    lua_context->initialize(lua_script_arg);
  }

  {
    FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
    if (dbg) { fprintf(dbg, "SOLARUS_DEBUG: [%u ms] P1\n", vita_ms()); fclose(dbg); }
#ifdef __vita__
    ::solarus_vita_splash_progress(0.97f);
#endif
  }
  if (lua_console_enabled) {
    Logger::info("Lua console: yes");
    initialize_lua_console();
  }
  else {
    Logger::info("Lua console: no");
  }
  {
    FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
    if (dbg) { fprintf(dbg, "SOLARUS_DEBUG: [%u ms] P2\n", vita_ms()); fclose(dbg); }
  }


  if (turbo) {
    Logger::info("Turbo mode: yes");
  }
  else {
    Logger::info("Turbo mode: no");
  }

  // Start loading resources in background.
#ifndef __vita__
  // On the Vita the memory is too small to preload every image.
  resource_provider.start_preloading_resources();
#endif
  {
    FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
    if (dbg) { fprintf(dbg, "SOLARUS_DEBUG: [%u ms] P3\n", vita_ms()); fclose(dbg); }
  }


  // Display the game icon as window icon (if any)
  setup_game_icon();
  {
    FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
    if (dbg) { fprintf(dbg, "SOLARUS_DEBUG: [%u ms] P4\n", vita_ms()); fclose(dbg); }
  }


  // Show the window.
  Video::show_window();
  {
    FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
    if (dbg) { fprintf(dbg, "SOLARUS_DEBUG: [%u ms] P5\n", vita_ms()); fclose(dbg); }
  }


  // Set the fullscreen mode if requested.
  const std::string& fullscreen_arg = args.get_argument_value("-fullscreen");
  if (!fullscreen_arg.empty()) {
    Video::set_fullscreen(fullscreen_arg == "yes");
  }

  // Set the mouse cursor visibility if requested.
  const std::string& cursor_visible_arg = args.get_argument_value("-cursor-visible");
  if (!cursor_visible_arg.empty()) {
    Video::set_cursor_visible(cursor_visible_arg == "yes");
  }
}

/**
 * \brief Cleans everything.
 */
MainLoop::~MainLoop() {

  if (game != nullptr) {
    game->stop();
    game.reset();  // While deleting the game, the Lua world must still exist.
  }

  // The resource provider holds sound buffers that may be still in-use.
  // Therefore, all sounds need to be stopped before clearing the resource provider.
  Sound::stop_all();

  resource_provider.clear();

  // Clear the surface while Lua still exists,
  // because it may point to other surfaces that have Lua movements.
  root_surface = nullptr;

  if (lua_context != nullptr) {
    lua_context->exit();
  }
  CurrentQuest::quit();
  QuestFiles::close_quest();
  System::quit();
  quit_lua_console();
}

/**
 * \brief Returns the shared Lua context.
 * \return The Lua context where all scripts are run.
 */
LuaContext& MainLoop::get_lua_context() {
  return *lua_context;
}

/**
 * \brief Returns the resource provider of this quest.
 * \return The resource provider.
 */
ResourceProvider& MainLoop::get_resource_provider() {
  return resource_provider;
}

/**
 * \brief Returns whether the simulation is suspended.
 *
 * \return true if the simulation is suspended
 */
bool MainLoop::is_suspended() {
  return suspended;
}

/**
 * \brief Suspends or resumes the simulation.
 *
 * \param suspended true to suspend the simulation, false to resume it
 */
void MainLoop::set_suspended(bool suspended) {

  if (suspended != this->suspended) {
    this->suspended = suspended;
  }
}

/**
 * \brief Returns whether the user just closed the window.
 *
 * When this function returns true, you should stop immediately
 * whatever you are doing, free your memory and let the program quit.
 *
 * \return true if the user wants to exit the program
 */
bool MainLoop::is_exiting() {
  return exiting;
}

/**
 * \brief Sets whether the user wants to quit the program.
 */
void MainLoop::set_exiting() {

  // Stop the program.
  exiting = true;
}

/**
 * \brief Returns whether the program is being reset.
 */
bool MainLoop::is_resetting() {

  return resetting;
}

/**
 * \brief Marks the current game as finished and sets the initial screen
 * to be started at the next cycle.
 */
void MainLoop::set_resetting() {

  resetting = true;
}

/**
 * \brief Returns the current game if any.
 * \return The game currently running or nullptr.
 */
Game* MainLoop::get_game() {
  return game.get();
}

/**
 * \brief Changes the game.
 * \param game The new game to start, or nullptr to start no game.
 */
void MainLoop::set_game(Game* game) {
  this->next_game = game;
}

/**
 * \brief Schedules a Lua command to be executed at the next cycle.
 *
 * This function is thread safe, it can be called from a separate thread
 * while the main loop is running.
 *
 * \param command The Lua string to execute.
 * \return A number identifying your command.
 */
int MainLoop::push_lua_command(const std::string& command) {

  std::lock_guard<std::mutex> lock(lua_commands_mutex);
  lua_commands.push_back(command);
  return num_lua_commands_pushed++;
}

/**
 * \brief Runs the main loop until the user requests to stop the program.
 *
 * The main loop controls simulated time and repeatedly updates the world and
 * redraws the screen.
 *
 * Does nothing if the quest is missing.
 */
void MainLoop::run() {
  SOL_MAIN_THREAD;
#ifdef __vita__
  vita_lock_ps(false);
#endif
#ifdef SOLARUS_PROFILING
  profiler::startListen();
#endif

  if (!QuestFiles::quest_exists()) {
    return;
  }

  // Main loop.
  Logger::info("Simulation started");

  if(CurrentQuest::get_properties().is_dynamic_timestep()) {
    dynamic_run();
  } else {
    fixed_run();
  }

  Logger::info("Simulation finished");
}

void MainLoop::dynamic_run() {
  uint64_t last_frame_date = System::get_real_time_ns();
  int64_t delta_buffer = 0; //Delta buffer is used to store difference between perfect timing and realtime

  auto delta_buffer_spill = 0.01f; //The lower it is the smoother we are but the less efficiently we catch up time
  // The main loop basically repeats
  // check_input(), update(), draw() and sleep().
  // Each call to update() makes the simulated time advance until next frame.

  while (!is_exiting()) {
    SOL_PBLOCK("Solarus::MainLoop::Frame");
    // 1. Detect and handle input events.
    check_input();

    if (!is_exiting() && !is_suspended()) {
      // Measure the time of the last iteration.
      uint64_t now = System::get_real_time_ns();
      int64_t last_frame_duration = now - last_frame_date;
      int64_t used_last_frame_duration = last_frame_duration;
      last_frame_date = now;

      int64_t perfect_frame_duration = Video::get_display_period_ns();

      if (last_frame_duration > perfect_frame_duration * 5) {
        //Do not try to catch up more than 10 frames, pretend everything is fine
        used_last_frame_duration = perfect_frame_duration;
      }

      //Compute how much we spill from delta_buffer
      int64_t spill = delta_buffer * delta_buffer_spill;

      //Transfer time to smoothed duration
      int64_t smoothed_duration = perfect_frame_duration + spill;

      //Put current frame error in buffer
      delta_buffer += used_last_frame_duration - smoothed_duration;

      // 2. Update the world once
      step(smoothed_duration);

      // 3. Draw
      draw();

      //Debug
      //std::cout << perfect_frame_duration << " " << last_frame_duration << " " << smoothed_duration << " " << delta_buffer << " " << spill << std::endl;
    }
    else
    {
      System::sleep(System::fixed_timestep_ns / 1000000);
    }
  }
}

void MainLoop::fixed_run() {
  uint64_t last_frame_date = System::get_real_time_ns();
  int64_t lag = 0;  // Lose time of the simulation to catch up.
  int64_t time_dropped = 0;  // Time that won't be caught up.

  // The main loop basically repeats
  // check_input(), update(), draw() and sleep().
  // Each call to update() makes the simulated time advance one fixed step.

  while (!is_exiting()) {
    SOL_PBLOCK("Solarus::MainLoop::Frame");
    // Measure the time of the last iteration.
    uint64_t now = System::get_real_time_ns() - time_dropped;
    uint64_t last_frame_duration = now - last_frame_date;
    last_frame_date = now;
    lag += last_frame_duration;
    // At this point, lag represents how much late the simulated time with
    // compared to the real time.

    if (lag >= 200000000) {
      // Huge lag: don't try to catch up.
      // Maybe we have just made a one-time heavy operation like loading a
      // big file, or the process was just unsuspended.
      // Let's fake the real time instead.
      time_dropped += lag - System::fixed_timestep_ns;
      lag = System::fixed_timestep_ns;
      last_frame_date = System::get_real_time_ns() - time_dropped;
    }

    // 1. Detect and handle input events.
    check_input();

    // 2. Update the world once, or several times (skipping some draws)
    // to catch up if the system is slow.
    int num_updates = 0;
    if (turbo && !is_suspended()) {
      // Turbo mode: always update at least once.
      step(System::fixed_timestep_ns);
      lag -= System::fixed_timestep_ns;
      ++num_updates;
    }

    while (lag >= static_cast<int64_t>(System::fixed_timestep_ns) &&
           num_updates < 10 && // To draw sometimes anyway on very slow systems.
           !is_exiting() && !is_suspended()
    ) {
      step(System::fixed_timestep_ns);
      lag -= System::fixed_timestep_ns;
      ++num_updates;
    }

    // 3. Redraw the screen.
    if (num_updates > 0 && !is_suspended()) {
      draw();
    }

    // 4. Sleep if we have time, to save CPU and GPU cycles.
    if (debug_lag > 0 && !turbo && !is_suspended()) {
      SOL_PBLOCK("Debug lag")
      // Extra sleep time for debugging, useful to simulate slower systems.
      System::sleep(debug_lag / 1000000);
    }

    last_frame_duration = (System::get_real_time_ms() - time_dropped) - last_frame_date;
    if (last_frame_duration < System::fixed_timestep_ns && !turbo) {
      SOL_PBLOCK("Timestep sleep");
      System::sleep(static_cast<uint32_t>((System::fixed_timestep_ns - last_frame_duration) / 1000000));
    }
  }
}

/**
 * \brief Advances the simulation of one tick.
 *
 * You can use this function if you want to simulate step by step.
 * Otherwise, use run() to execute the standard main loop.
 */
void MainLoop::step(uint64_t timestep_ns) {
  SOL_PFUN();
#ifdef __vita__
  vita_first_step_check();
  if (vita_menu_is_open()) {
    return;  // Engine menu open: the game is frozen.
  }
#endif
  if (game != nullptr) {
    game->update();
  }
#ifdef __vita__
  solarus_vita_layout_step();
#endif
  lua_context->update();
  System::update(timestep_ns);

  // Cleanup the game if we are resetting.
  if (resetting) {
    if (game != nullptr) {
      game->stop();
    }
    set_game(nullptr);
    resetting = false;
  }

  // Go to another game?
  if (next_game != game.get()) {

#ifdef __vita__
    if (next_game != nullptr) {
      vita_lock_ps(true);
    }
#endif
    game = std::unique_ptr<Game>(next_game);

    if (game != nullptr) {
      game->start();
#ifdef __vita__
      vita_lock_ps(false);
      solarus_vita_layout_start();
#endif
    }
    else {
      // Reset
      lua_context->exit();
      lua_context->initialize(lua_script_arg);
      MusicSystem::stop_playing();
    }
  }
}

/**
 * \brief Detects whether there were input events and if yes, handles them.
 */
void MainLoop::check_input() {
  SOL_PFUN();
  // Check SDL events.
  for(std::unique_ptr<InputEvent> event =  InputEvent::get_event();
      event != nullptr;
      event = InputEvent::get_event()) {
     notify_input(*event);
  }

  // Check Lua requests.
  if (lua_console_enabled && !lua_commands.empty()) {
    std::lock_guard<std::mutex> lock(lua_commands_mutex);
    for (const std::string& command : lua_commands) {
      std::cout << "\n";  // To make sure that the command delimiter starts on a new line.
      Logger::info("====== Begin Lua command #" + String::to_string(num_lua_commands_done) + " ======");
      const bool success = get_lua_context().do_string_with_easy_env(command, "Lua command");
      if (success) {
        std::cout << "\n";
        Logger::info("====== End Lua command #" + String::to_string(num_lua_commands_done) + ": success ======");
      }
      else {
        std::cout << "\n";
        Logger::info("====== End Lua command #" + String::to_string(num_lua_commands_done) + ": error ======");
      }
      ++num_lua_commands_done;
    }
    lua_commands.clear();
  }
}

void MainLoop::setup_game_icon() {
  static const std::vector<std::string> file_names = {
    "logos/icon_1024.png",
    "logos/icon_512.png",
    "logos/icon_256.png",
    "logos/icon_128.png",
    "logos/icon_64.png",
    "logos/icon_48.png",
    "logos/icon_32.png",
    "logos/icon_24.png",
    "logos/icon_16.png"
  };

  for(const auto& file : file_names) {
    SDL_Surface_UniquePtr surface = Surface::create_sdl_surface_from_file(file);
    if(surface) {
      Video::set_window_icon(surface.get());
      return;
    }
  }

  //else try to use default icon
  SDL_Surface_UniquePtr surface = Surface::create_sdl_surface_from_memory(quest_icon_data, quest_icon_data_len);
  SOLARUS_REQUIRE(bool(surface), "Could not load built-in icon");

  Video::set_window_icon(surface.get());
}

/**
 * \brief This function is called when there is an input event.
 *
 * It forwards the event to Lua, and then to the game if Lua did not
 * handle it.
 */
#ifdef __vita__
void solarus_vita_toggle_stretch();
void solarus_vita_toggle_legend(const std::string& text);
#endif

#ifdef __vita__
void solarus_vita_overlay_set(const std::vector<std::string>& lines);

namespace {

// ===== Vita engine menu (Select) =====
// The engine turns the pad into keyboard keys for every game, using one
// mapping per game (ux0:data/Solarus/keymaps/<quest>.txt). Select opens a
// menu, drawn over the game, to change that mapping.

struct VitaKeyChoice {
  std::string name;
  std::string label;
  SDL_Keycode key;
};

const std::vector<VitaKeyChoice>& vita_key_choices() {
  static std::vector<VitaKeyChoice> choices;
  if (choices.empty()) {
    choices.push_back({ "none", "AUCUNE", SDLK_UNKNOWN });
    choices.push_back({ "space", "ESPACE", SDLK_SPACE });
    choices.push_back({ "return", "ENTREE", SDLK_RETURN });
    choices.push_back({ "escape", "ECHAP", SDLK_ESCAPE });
    choices.push_back({ "tab", "TAB", SDLK_TAB });
    for (char c = 'a'; c <= 'z'; ++c) {
      choices.push_back({ std::string(1, c), std::string(1, static_cast<char>(c - 'a' + 'A')), static_cast<SDL_Keycode>(SDLK_a + (c - 'a')) });
    }
    for (char c = '0'; c <= '9'; ++c) {
      choices.push_back({ std::string(1, c), std::string(1, c), static_cast<SDL_Keycode>(SDLK_0 + (c - '0')) });
    }
  }
  return choices;
}

struct VitaPadButton {
  const char* id;
  const char* label;
  int button;
  SDL_Keycode default_key;
};

const VitaPadButton vita_pad_buttons[] = {
  { "a",     "CROIX",    SDL_CONTROLLER_BUTTON_A,            SDLK_SPACE },
  { "b",     "ROND",     SDL_CONTROLLER_BUTTON_B,            SDLK_c },
  { "x",     "CARRE",    SDL_CONTROLLER_BUTTON_X,            SDLK_x },
  { "y",     "TRIANGLE", SDL_CONTROLLER_BUTTON_Y,            SDLK_v },
  { "l",     "L",        SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDLK_RETURN },
  { "start", "START",    SDL_CONTROLLER_BUTTON_START,        SDLK_d },
  { "select", "SELECT",  SDL_CONTROLLER_BUTTON_BACK,         SDLK_UNKNOWN }
};
const int vita_num_pad_buttons = 7;

SDL_Keycode vita_pad_key[7];
bool vita_select_down = false;
bool vita_l_down = false;
bool vita_map_ready = false;
bool vita_map_exists = false;
bool vita_pad_mode = true;  // true: the game receives the Vita buttons itself.
int vita_layout_countdown = -1;

// VITA_LAYOUT: Cross = action, Circle = attack, Square = item 1,
// Triangle = item 2, Start = pause (the face buttons of the Vita).
void vita_apply_layout() {
  if (vita_main_loop == nullptr || vita_main_loop->get_game() == nullptr) {
    return;
  }
  Game& g = *vita_main_loop->get_game();
  Controls& c = g.get_controls();
  c.set_joypad_binding(CommandId::ACTION, Controls::JoypadBinding("a"));
  c.set_joypad_binding(CommandId::ATTACK, Controls::JoypadBinding("b"));
  c.set_joypad_binding(CommandId::ITEM_1, Controls::JoypadBinding("x"));
  c.set_joypad_binding(CommandId::ITEM_2, Controls::JoypadBinding("y"));
  c.set_joypad_binding(CommandId::PAUSE, Controls::JoypadBinding("start"));
  g.get_savegame().set_boolean("_vita_layout", true);
}

enum VitaMenuMode { VITA_MENU_CLOSED, VITA_MENU_MAIN, VITA_MENU_COMMANDS, VITA_MENU_PROMPT, VITA_MENU_CONFIRM };
VitaMenuMode vita_menu_mode = VITA_MENU_CLOSED;
int vita_menu_sel = 0;
std::set<SDL_Keycode> vita_keys_down;
int vita_stick_state[2] = { 0, 0 };

std::string vita_keymap_path() {
  return "ux0:data/Solarus/keymaps/" + vita_quest_id + ".txt";
}

bool vita_is_launcher() {
  return vita_quest_id == "launcher" || vita_quest_id.empty();
}

void vita_defaults() {
  for (int i = 0; i < vita_num_pad_buttons; ++i) {
    vita_pad_key[i] = vita_pad_buttons[i].default_key;
    if (vita_is_launcher()) {
      // The game list: Cross = validate, Circle = cancel.
      vita_pad_key[i] = SDLK_UNKNOWN;
    }
  }
  if (vita_is_launcher()) {
    vita_pad_key[0] = SDLK_RETURN;
    vita_pad_key[1] = SDLK_ESCAPE;
    vita_pad_key[3] = SDLK_i;  // Triangle: more info about the game.
  }
}

int vita_choice_index(SDL_Keycode key) {
  const std::vector<VitaKeyChoice>& choices = vita_key_choices();
  for (size_t i = 0; i < choices.size(); ++i) {
    if (choices[i].key == key) {
      return static_cast<int>(i);
    }
  }
  return 0;
}

void vita_load_map() {
  vita_defaults();
  vita_map_exists = false;
  vita_pad_mode = !vita_is_launcher();
  if (vita_is_launcher()) {
    return;  // Fixed mapping for the game list.
  }
  FILE* f = fopen(vita_keymap_path().c_str(), "r");
  if (!f) {
    return;
  }
  vita_map_exists = true;
  char line[128];
  while (fgets(line, sizeof(line), f)) {
    char* eq = strchr(line, '=');
    if (!eq || eq == line) {
      continue;
    }
    *eq = 0;
    char* value = eq + 1;
    for (char* p = value; *p; ++p) {
      if (*p == '\r' || *p == '\n') {
        *p = 0;
      }
    }
    if (strcmp(line, "mode") == 0) {
      vita_pad_mode = (strcmp(value, "keys") != 0);
      continue;
    }
    SDL_Keycode k = SDL_GetKeyFromName(value);
    if (strcmp(value, "none") == 0) {
      k = SDLK_UNKNOWN;
    }
    else if (k == SDLK_UNKNOWN) {
      continue;
    }
    for (int i = 0; i < vita_num_pad_buttons; ++i) {
      if (strcmp(line, vita_pad_buttons[i].id) == 0) {
        vita_pad_key[i] = k;
      }
    }
  }
  fclose(f);
}

void vita_save_map() {
  sceIoMkdir("ux0:data/Solarus", 0777);
  sceIoMkdir("ux0:data/Solarus/keymaps", 0777);
  FILE* f = fopen(vita_keymap_path().c_str(), "w");
  if (!f) {
    return;
  }
  fprintf(f, "mode=%s\n", vita_pad_mode ? "pad" : "keys");
  for (int i = 0; i < vita_num_pad_buttons; ++i) {
    const char* name = "none";
    if (vita_pad_key[i] != SDLK_UNKNOWN) {
      name = SDL_GetKeyName(vita_pad_key[i]);
    }
    fprintf(f, "%s=%s\n", vita_pad_buttons[i].id, name);
  }
  fclose(f);
  vita_map_exists = true;
}

void vita_ensure_map() {
  if (!vita_map_ready) {
    vita_map_ready = true;
    vita_load_map();
  }
}

void vita_push_key(SDL_Keycode key, bool down) {
  if (key == SDLK_UNKNOWN) {
    return;
  }
  if (down) {
    vita_keys_down.insert(key);
  }
  else {
    vita_keys_down.erase(key);
  }
  SDL_Event e;
  SDL_zero(e);
  e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
  e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
  e.key.repeat = 0;
  e.key.keysym.sym = key;
  e.key.keysym.scancode = SDL_GetScancodeFromKey(key);
  e.key.keysym.mod = KMOD_NONE;
  SDL_PushEvent(&e);
}

void vita_release_all_keys() {
  const std::set<SDL_Keycode> pressed = vita_keys_down;
  for (SDL_Keycode k : pressed) {
    vita_push_key(k, false);
  }
  vita_stick_state[0] = 0;
  vita_stick_state[1] = 0;
}

void vita_stick(int index, double value, SDL_Keycode negative, SDL_Keycode positive) {
  const int old_state = vita_stick_state[index];
  int new_state = old_state;
  if (old_state == 0) {
    if (value < -0.5) { new_state = -1; }
    else if (value > 0.5) { new_state = 1; }
  }
  else if (old_state < 0 && value > -0.3) {
    new_state = (value > 0.5) ? 1 : 0;
  }
  else if (old_state > 0 && value < 0.3) {
    new_state = (value < -0.5) ? -1 : 0;
  }
  if (new_state == old_state) {
    return;
  }
  if (old_state < 0) { vita_push_key(negative, false); }
  if (old_state > 0) { vita_push_key(positive, false); }
  if (new_state < 0) { vita_push_key(negative, true); }
  if (new_state > 0) { vita_push_key(positive, true); }
  vita_stick_state[index] = new_state;
}

std::string vita_pad_name(const char* text, size_t width) {
  std::string s = text;
  while (s.size() < width) {
    s += ' ';
  }
  return s;
}

std::vector<int> vita_main_items() {
  std::vector<int> items;
  items.push_back(0);
  // (1 = commands screen: removed, the controls are set in each game.)
  if (!vita_is_launcher()) {
    items.push_back(2);  // Size of the game on the screen.
    items.push_back(4);  // Display filter.
    items.push_back(3);  // Back to the game list.
  }
  return items;
}

// Commands screen items: mode, [7 buttons in keyboard mode], defaults, back.
int vita_cmd_count() {
  return vita_pad_mode ? 3 : vita_num_pad_buttons + 3;
}

void vita_toggle_mode() {
  vita_release_all_keys();
  vita_pad_mode = !vita_pad_mode;
  vita_save_map();
  vita_menu_sel = 0;
}

// Size and filter of the display, kept for each game.
const char* vita_size_names[] = { "CENTRE", "AGRANDI", "ETIRE" };
const char* vita_filter_names[] = { "NET", "LISSE", "LIGNES", "GRILLE" };

std::string vita_display_path() {
  return "ux0:data/Solarus/display/" + vita_quest_id + ".txt";
}

void vita_save_display() {
  sceIoMkdir("ux0:data/Solarus", 0777);
  sceIoMkdir("ux0:data/Solarus/display", 0777);
  FILE* f = fopen(vita_display_path().c_str(), "w");
  if (f) {
    fprintf(f, "size=%d\nfilter=%d\n", Video::get_vita_size_mode(), Video::get_vita_filter());
    fclose(f);
  }
}

void vita_load_display() {
  int size_mode = 1;  // Enlarged by default.
  int filter = 0;     // Sharp pixels by default.
  FILE* f = fopen(vita_display_path().c_str(), "r");
  if (f) {
    char line[64];
    while (fgets(line, sizeof(line), f)) {
      int value = 0;
      if (sscanf(line, "size=%d", &value) == 1) {
        size_mode = value;
      }
      else if (sscanf(line, "filter=%d", &value) == 1) {
        filter = value;
      }
    }
    fclose(f);
  }
  Video::set_vita_display(size_mode, filter);
}

void vita_menu_refresh() {
  std::vector<std::string> lines;
  const std::string cursor = "> ";
  const std::string none = "  ";
  if (vita_menu_mode == VITA_MENU_CLOSED) {
    solarus_vita_overlay_set(lines);
    return;
  }
  solarus_vita_menu_show_background = true;
  if (vita_menu_mode == VITA_MENU_MAIN) {
    {
      // Choosing the size or the filter: show the game behind the menu.
      const std::vector<int> main_items = vita_main_items();
      const int item = main_items[vita_menu_sel];
      solarus_vita_menu_show_background = (item != 2 && item != 4);
    }
    lines.push_back("MENU SOLARUS");
    lines.push_back("");
    const std::vector<int> items = vita_main_items();
    for (size_t i = 0; i < items.size(); ++i) {
      const char* label = "";
      switch (items[i]) {
        case 0: label = vita_is_launcher() ? "REPRENDRE" : "REPRENDRE LA PARTIE"; break;
        case 1: label = "COMMANDES DE CE JEU"; break;
        case 2: label = nullptr; break;
        case 4: label = nullptr; break;
        default: label = "RETOUR A LA LISTE DES JEUX"; break;
      }
      std::string text;
      if (items[i] == 2) {
        text = std::string("TAILLE : < ") + vita_size_names[Video::get_vita_size_mode()] + " >";
      }
      else if (items[i] == 4) {
        text = std::string("FILTRE : < ") + vita_filter_names[Video::get_vita_filter()] + " >";
      }
      else {
        text = label;
      }
      lines.push_back((vita_menu_sel == static_cast<int>(i) ? cursor : none) + text);
    }
    lines.push_back("");
    lines.push_back("~HAUT BAS CHOISIR   GAUCHE DROITE CHANGER");
    lines.push_back("~CROIX VALIDER   ROND RETOUR   R TAILLE");
  }
  else if (vita_menu_mode == VITA_MENU_COMMANDS) {
    std::string id = vita_quest_id;
    if (id.size() > 28) {
      id = id.substr(0, 28);
    }
    lines.push_back("COMMANDES DE " + id);
    lines.push_back("");
    const std::vector<VitaKeyChoice>& choices = vita_key_choices();
    lines.push_back((vita_menu_sel == 0 ? cursor : none) +
        std::string(vita_pad_mode ? "MODE : MANETTE" : "MODE : CLAVIER"));
    if (vita_pad_mode) {
      lines.push_back("~LE JEU RECOIT LES BOUTONS VITA");
      lines.push_back("~REGLAGE DANS LE MENU CONTROLES DU JEU");
    }
    else {
      for (int i = 0; i < vita_num_pad_buttons; ++i) {
        lines.push_back((vita_menu_sel == i + 1 ? cursor : none) +
            vita_pad_name(vita_pad_buttons[i].label, 10) + ": " +
            choices[vita_choice_index(vita_pad_key[i])].label);
      }
    }
    const int n = vita_cmd_count();
    lines.push_back((vita_menu_sel == n - 2 ? cursor : none) +
        std::string(vita_pad_mode ? "DISPOSITION VITA : CROIX ACTION" : "REMETTRE LES COMMANDES PAR DEFAUT"));
    if (vita_pad_mode) {
      lines.push_back("~ROND EPEE   CARRE ET TRIANGLE OBJETS");
    }
    lines.push_back((vita_menu_sel == n - 1 ? cursor : none) + "RETOUR");
    lines.push_back("");
    lines.push_back("~GAUCHE DROITE CHANGER   ROND RETOUR");
    lines.push_back("~LES FLECHES ET LE STICK GAUCHE SONT FIXES");
  }
  else if (vita_menu_mode == VITA_MENU_PROMPT) {
    lines.push_back("PREMIER LANCEMENT DE CE JEU");
    lines.push_back("");
    lines.push_back("REGLER LES COMMANDES MAINTENANT ?");
    lines.push_back("");
    lines.push_back((vita_menu_sel == 0 ? cursor : none) + "OUI REGLER MAINTENANT");
    lines.push_back((vita_menu_sel == 1 ? cursor : none) + "NON GARDER LES COMMANDES PAR DEFAUT");
    lines.push_back("");
    lines.push_back("~SELECT + L OUVRE CE MENU A TOUT MOMENT");
  }
  else if (vita_menu_mode == VITA_MENU_CONFIRM) {
    lines.push_back("QUITTER CE JEU ?");
    lines.push_back("");
    lines.push_back("LA PARTIE NON SAUVEGARDEE SERA PERDUE");
    lines.push_back("");
    lines.push_back((vita_menu_sel == 0 ? cursor : none) + "NON RESTER DANS LE JEU");
    lines.push_back((vita_menu_sel == 1 ? cursor : none) + "OUI RETOUR A LA LISTE");
  }
  solarus_vita_overlay_set(lines);
}

void vita_menu_open(VitaMenuMode mode) {
  if (vita_menu_mode == VITA_MENU_CLOSED) {
    vita_release_all_keys();
    Sound::pause_all();
    MusicSystem::pause_playing();
  }
  vita_menu_mode = mode;
  vita_menu_sel = 0;
  vita_menu_refresh();
}

void vita_menu_close() {
  vita_menu_mode = VITA_MENU_CLOSED;
  vita_menu_refresh();
  MusicSystem::resume_playing();
  Sound::resume_all();
}

int vita_menu_item_count() {
  switch (vita_menu_mode) {
    case VITA_MENU_MAIN: return static_cast<int>(vita_main_items().size());
    case VITA_MENU_COMMANDS: return vita_cmd_count();
    case VITA_MENU_PROMPT: return 2;
    case VITA_MENU_CONFIRM: return 2;
    default: return 0;
  }
}

void vita_menu_change_key(int index, int delta) {
  const int count = static_cast<int>(vita_key_choices().size());
  int choice = vita_choice_index(vita_pad_key[index]) + delta;
  choice = ((choice % count) + count) % count;
  vita_pad_key[index] = vita_key_choices()[choice].key;
  vita_save_map();
}

void vita_menu_validate() {
  switch (vita_menu_mode) {
    case VITA_MENU_MAIN: {
      const std::vector<int> items = vita_main_items();
      const int what = items[vita_menu_sel];
      if (what == 0) {
        vita_menu_close();
      }
      else if (what == 1) {
        vita_menu_open(VITA_MENU_COMMANDS);
      }
      else if (what == 2) {
        Video::cycle_vita_size(1);
        vita_save_display();
      }
      else if (what == 4) {
        Video::cycle_vita_filter(1);
        vita_save_display();
      }
      else {
        vita_menu_open(VITA_MENU_CONFIRM);
      }
      break;
    }
    case VITA_MENU_COMMANDS:
      if (vita_menu_sel == 0) {
        vita_toggle_mode();
      }
      else if (vita_menu_sel < vita_cmd_count() - 2) {
        vita_menu_change_key(vita_menu_sel - 1, 1);
      }
      else if (vita_menu_sel == vita_cmd_count() - 2) {
        if (vita_pad_mode) {
          vita_apply_layout();
          vita_menu_close();
          return;
        }
        vita_defaults();
        vita_save_map();
      }
      else {
        vita_menu_open(VITA_MENU_MAIN);
      }
      break;
    case VITA_MENU_PROMPT:
      vita_save_map();
      if (vita_menu_sel == 0) {
        vita_menu_open(VITA_MENU_COMMANDS);
      }
      else {
        vita_menu_close();
      }
      break;
    case VITA_MENU_CONFIRM:
      if (vita_menu_sel == 1) {
        remove("ux0:data/Solarus/mode.txt");
        sceAppMgrLoadExec("app0:eboot.bin", nullptr, nullptr);
      }
      else {
        vita_menu_open(VITA_MENU_MAIN);
        vita_menu_sel = static_cast<int>(vita_main_items().size()) - 1;
      }
      break;
    default:
      break;
  }
  vita_menu_refresh();
}

void vita_menu_back() {
  switch (vita_menu_mode) {
    case VITA_MENU_MAIN:
      vita_menu_close();
      break;
    case VITA_MENU_COMMANDS:
      vita_menu_open(VITA_MENU_MAIN);
      break;
    case VITA_MENU_PROMPT:
      vita_save_map();
      vita_menu_close();
      break;
    case VITA_MENU_CONFIRM:
      vita_menu_open(VITA_MENU_MAIN);
      vita_menu_sel = static_cast<int>(vita_main_items().size()) - 1;
      break;
    default:
      break;
  }
  vita_menu_refresh();
}

// Returns true if the event was used by the engine menu.
bool vita_menu_event(const InputEvent& event) {
  const bool pressed = event.is_joypad_button_pressed();
  const bool released = event.is_joypad_button_released();
  const int button = (pressed || released) ? static_cast<int>(event.get_joypad_button()) : -1;

  if (button == SDL_CONTROLLER_BUTTON_BACK) {
    vita_select_down = pressed;
  }
  if (button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
    vita_l_down = pressed;
  }

  if (vita_menu_mode == VITA_MENU_CLOSED) {
    // Select + L (either order) opens the engine menu. Every button, Select
    // and L included, is otherwise a normal game button. R is the
    // stretched / centered display switch.
    if (pressed &&
        ((button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER && vita_select_down) ||
         (button == SDL_CONTROLLER_BUTTON_BACK && vita_l_down))) {
      vita_ensure_map();
      vita_menu_open(VITA_MENU_MAIN);
      return true;
    }
    return false;
  }

  if (!event.is_joypad_event()) {
    return false;
  }
  if (!pressed) {
    if (vita_pad_mode && released &&
        button != SDL_CONTROLLER_BUTTON_DPAD_UP && button != SDL_CONTROLLER_BUTTON_DPAD_DOWN &&
        button != SDL_CONTROLLER_BUTTON_DPAD_LEFT && button != SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
      return false;  // Let the game see the release of a button held before the menu.
    }
    switch (button) {
      case SDL_CONTROLLER_BUTTON_DPAD_UP: vita_push_key(SDLK_UP, false); break;
      case SDL_CONTROLLER_BUTTON_DPAD_DOWN: vita_push_key(SDLK_DOWN, false); break;
      case SDL_CONTROLLER_BUTTON_DPAD_LEFT: vita_push_key(SDLK_LEFT, false); break;
      case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: vita_push_key(SDLK_RIGHT, false); break;
      default: break;
    }
    return true;
  }
  const int count = vita_menu_item_count();
  switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
      vita_menu_sel = (vita_menu_sel + count - 1) % count;
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
      vita_menu_sel = (vita_menu_sel + 1) % count;
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      if (vita_menu_mode == VITA_MENU_MAIN) {
        const int delta = (button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) ? -1 : 1;
        const std::vector<int> items = vita_main_items();
        if (items[vita_menu_sel] == 2) {
          Video::cycle_vita_size(delta);
          vita_save_display();
        }
        else if (items[vita_menu_sel] == 4) {
          Video::cycle_vita_filter(delta);
          vita_save_display();
        }
      }
      if (vita_menu_mode == VITA_MENU_COMMANDS) {
        if (vita_menu_sel == 0) {
          vita_toggle_mode();
        }
        else if (vita_menu_sel < vita_cmd_count() - 2) {
          vita_menu_change_key(vita_menu_sel - 1,
              button == SDL_CONTROLLER_BUTTON_DPAD_LEFT ? -1 : 1);
        }
      }
      break;
    case SDL_CONTROLLER_BUTTON_A:
      vita_menu_validate();
      return true;
    case SDL_CONTROLLER_BUTTON_B:
    case SDL_CONTROLLER_BUTTON_BACK:
      vita_menu_back();
      return true;
    default:
      break;
  }
  vita_menu_refresh();
  return true;
}

// Turns a pad event into keyboard keys. Returns true if the event was
// converted: the game never sees the pad itself.
bool vita_pad_event(const InputEvent& event) {
  vita_ensure_map();
  const bool pressed = event.is_joypad_button_pressed();
  if (pressed || event.is_joypad_button_released()) {
    const int b = static_cast<int>(event.get_joypad_button());
    if (b == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
      return false;  // Reserved: stretched or centered display.
    }
    if (vita_pad_mode) {
      return false;  // Pad mode: the game receives the Vita buttons itself.
    }
    SDL_Keycode k = SDLK_UNKNOWN;
    switch (b) {
      case SDL_CONTROLLER_BUTTON_DPAD_UP: k = SDLK_UP; break;
      case SDL_CONTROLLER_BUTTON_DPAD_DOWN: k = SDLK_DOWN; break;
      case SDL_CONTROLLER_BUTTON_DPAD_LEFT: k = SDLK_LEFT; break;
      case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: k = SDLK_RIGHT; break;
      default:
        for (int i = 0; i < vita_num_pad_buttons; ++i) {
          if (vita_pad_buttons[i].button == b) {
            k = vita_pad_key[i];
          }
        }
        break;
    }
    vita_push_key(k, pressed);
    return true;
  }
  if (event.is_joypad_axis_moved()) {
    const int axis = static_cast<int>(event.get_joypad_axis());
    const double value = event.get_joypad_axis_state();
    if (axis == SDL_CONTROLLER_AXIS_LEFTX) {
      vita_stick(0, value, SDLK_LEFT, SDLK_RIGHT);
    }
    else if (axis == SDL_CONTROLLER_AXIS_LEFTY) {
      vita_stick(1, value, SDLK_UP, SDLK_DOWN);
    }
    return true;
  }
  return false;
}

}  // namespace

// Called at each step: asks about the controls the first time a game runs.
void vita_first_step_check() {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;
  {
    FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
    if (dbg) { fprintf(dbg, "SOLARUS_DEBUG: [%u ms] PREMIERE IMAGE DU JEU\n", vita_ms()); fclose(dbg); }
  }
  vita_hide_loading();
  ::solarus_vita_splash_progress(1.0f);
  vita_splash_frames = 0;
  vita_ensure_map();
  if (!vita_is_launcher()) {
    vita_load_display();  // Size and filter chosen before for this game.
  }
  // (No more "set the controls now?" question: the Vita layout is applied
  // automatically and the controls are set in each game's own menu.)
}

bool vita_menu_is_open() {
  return vita_menu_mode != VITA_MENU_CLOSED;
}

// True when the current game receives the Vita buttons itself (pad mode).
bool solarus_vita_pad_mode_active() {
  vita_ensure_map();
  return vita_pad_mode && !vita_is_launcher();
}

// When a game starts, the Vita layout is applied once per savegame, half a
// second later (after the quest scripts have set their own bindings).
void solarus_vita_layout_start() {
  vita_layout_countdown = 30;
}

void solarus_vita_layout_step() {
  if (vita_layout_countdown > 0 && --vita_layout_countdown == 0 &&
      vita_main_loop != nullptr && vita_main_loop->get_game() != nullptr) {
    Savegame& sg = vita_main_loop->get_game()->get_savegame();
    if (solarus_vita_pad_mode_active() &&
        !(sg.is_boolean("_vita_layout") && sg.get_boolean("_vita_layout"))) {
      vita_apply_layout();
    }
  }
}
#endif


void MainLoop::notify_input(const InputEvent& event) {
#ifdef __vita__
  if (vita_menu_event(event)) {
    return;
  }
  // R button: next size of the game (centered, enlarged, stretched).
  if (event.is_joypad_button_pressed() &&
      event.get_joypad_button() == JoyPadButton::RIGHT_SHOULDER) {
    if (!vita_is_launcher()) {
      Video::cycle_vita_size(1);
      vita_save_display();
    }
    return;
  }
  // Select button: show or hide the controls legend.
  if (false && event.is_joypad_button_pressed() &&
      event.get_joypad_button() == JoyPadButton::BACK) {
    std::string text = "PAS DE PARTIE EN COURS";
    if (game != nullptr) {
      auto name_of = [&](CommandId id) -> std::string {
        auto b = game->get_controls().get_joypad_binding(id);
        if (!b) {
          return "?";
        }
        if (std::holds_alternative<JoyPadButton>(*b)) {
          switch (std::get<JoyPadButton>(*b)) {
            case JoyPadButton::A: return "CROSS";
            case JoyPadButton::B: return "CIRCLE";
            case JoyPadButton::X: return "SQUARE";
            case JoyPadButton::Y: return "TRIANGLE";
            case JoyPadButton::BACK: return "SELECT";
            case JoyPadButton::START: return "START";
            case JoyPadButton::LEFT_SHOULDER: return "L";
            case JoyPadButton::RIGHT_SHOULDER: return "R";
            case JoyPadButton::DPAD_UP: return "UP";
            case JoyPadButton::DPAD_DOWN: return "DOWN";
            case JoyPadButton::DPAD_LEFT: return "LEFT";
            case JoyPadButton::DPAD_RIGHT: return "RIGHT";
            default: return "?";
          }
        }
        return "STICK";
      };
      text = "ACTION=" + name_of(CommandId::ACTION) +
             "  ATTAQUE=" + name_of(CommandId::ATTACK) +
             "  OBJET1=" + name_of(CommandId::ITEM_1) +
             "  OBJET2=" + name_of(CommandId::ITEM_2) +
             "  PAUSE=" + name_of(CommandId::PAUSE);
    }
    solarus_vita_toggle_legend(text);
    return;
  }
  // D-pad: the engine decides. A screen that has an on_joypad_hat_moved()
  // handler receives it; for every other screen the engine sends the
  // matching arrow key, once.
  if (!solarus_vita_pad_mode_active() && event.is_joypad_event() && event.get_dpad_direction() != -2) {
    if (!lua_context->notify_input(event)) {
      const int d = event.get_dpad_direction();
      const bool down = event.is_joypad_button_pressed();
      SDL_Keycode k = (d == 0) ? SDLK_RIGHT : (d == 2) ? SDLK_UP : (d == 4) ? SDLK_LEFT : SDLK_DOWN;
      SDL_Event ke;
      SDL_zero(ke);
      ke.type = down ? SDL_KEYDOWN : SDL_KEYUP;
      ke.key.state = down ? SDL_PRESSED : SDL_RELEASED;
      ke.key.repeat = 0;
      ke.key.keysym.sym = k;
      ke.key.keysym.scancode = SDL_GetScancodeFromKey(k);
      ke.key.keysym.mod = KMOD_NONE;
      SDL_PushEvent(&ke);
    }
    return;
  }
  // Every other pad button and the left stick become keys, using the
  // mapping chosen for this game in the engine menu.
  if (vita_pad_event(event)) {
    return;
  }
#endif

  bool handled = false;
  if (event.is_window_closing()) {
#ifdef __vita__
    // On the Vita the system suspends or kills the app itself: never
    // quit on a close request, so that pressing PS keeps the game.
    FILE* quit_log = fopen("ux0:data/solarus_debug.log", "a");
    if (quit_log) {
      fprintf(quit_log, "SOLARUS_DEBUG: [%u ms] window close request ignored\n", vita_ms());
      fclose(quit_log);
    }
#else
    set_exiting();
#endif
  }
  else if (event.is_window_resizing()) {
    // Let video module resize it's geometry
    Video::on_window_resized(event.get_window_size());
    //Notify game if any
    make_root_surface();
    if(game) {
      game->notify_window_size_changed(event.get_window_size());
    }
  }
  else if (suspend_unfocused && event.is_window_focus_lost()) {
    if (!is_suspended()) {
      Logger::info("Simulation suspended");
      set_suspended(true);
      Sound::pause_all();
      MusicSystem::pause_playing();
    }
  }
  else if (suspend_unfocused && event.is_window_focus_gained()) {
    if (is_suspended()) {
      Logger::info("Simulation resumed");
      set_suspended(false);
      MusicSystem::resume_playing();
      Sound::resume_all();
    }
  }
  else if (event.is_keyboard_key_pressed()) {
    // A key was pressed.
#if defined(PANDORA)
    // TODO make a clean flag
    if (event.get_keyboard_key() == InputEvent::KEY_ESCAPE) {
      exiting = true;
    }
#endif
  } else if (event.is_controller_event()) {
    // First check if main joypad disconnected
    if(InputEvent::is_legacy_joypad_enabled() &&
       event.is_joypad_removed() &&
       game &&
       (game->get_controls().get_joypad() == event.get_joypad())) {
      // Main controls joypad is removed, try to fallback on another joypad
      auto new_joy = InputEvent::other_joypad(event.get_joypad());
      if(new_joy) {
        Logger::info("Using joystick: \"" + new_joy->get_name() + "\"");
      }
      game->get_controls().set_joypad(new_joy); //Could set joypad to nullptr, leaving it without joy
    }

    handled = event.notify_joypad(*lua_context);

    if(InputEvent::is_legacy_joypad_enabled() && event.is_joypad_added() && game && !game->get_controls().get_joypad()) {
      // A joypad was connected and main commands did not had a joypad
      auto new_joy = event.get_joypad();
      if(new_joy) {
        Logger::info("Using joystick: \"" + new_joy->get_name() + "\"");
      }
      game->get_controls().set_joypad(new_joy);
    }
  }

  // Send the event to Lua and to the current screen.
  if(!handled) {
    handled = lua_context->notify_input(event);
  }

  if (!handled && game != nullptr) {
    handled = game->notify_input(event);
  }

  if(!handled) {
    commands_dispatcher.notify_input(event);
  }
}

void MainLoop::notify_control(const ControlEvent& event) {

  if (lua_context->notify_control(event)) {
    return;
  }

  if (game != nullptr) {
    game->notify_control(event);
  }
}

/**
 * \brief Redraws the current screen.
 *
 * This function is called repeatedly by the main loop.
 */
void MainLoop::draw() {
  SOL_PFUN();
  root_surface->clear();
  Video::clear_screen_surface();

  if (game != nullptr) {
    game->draw(root_surface, Video::get_screen_surface());
  }
  lua_context->main_on_draw(root_surface);
  Video::render(root_surface);
  lua_context->video_on_draw(Video::get_screen_surface());
  Video::finish();
#ifdef __vita__
  if (vita_splash_frames >= 0 && ++vita_splash_frames >= 2) {
    vita_splash_frames = -1;
    ::solarus_vita_splash_free();
  }
#endif
}

/**
 * \brief Reads the quest properties file quest.dat and applies its settings.
 */
void MainLoop::load_quest_properties() {

  const QuestProperties& properties = CurrentQuest::get_properties();

  check_version_compatibility(properties.get_solarus_version_major_minor());

  Logger::info("Quest format: " + properties.get_solarus_version());

  const std::string& title = properties.get_title();
  const std::string& quest_version = properties.get_quest_version();

  if (!title.empty()) {
    std::string window_title = title;
    if (!quest_version.empty()) {
      window_title += " " + quest_version;
    }
#if defined(SOLARUS_GIT_REVISION) && !defined(NDEBUG)
    window_title += " - Solarus " SOLARUS_VERSION " (" SOLARUS_GIT_REVISION ")";
#else
    window_title += " - Solarus " SOLARUS_VERSION;
#endif
    Video::set_window_title(window_title);
  }

  Video::set_quest_size_range(
      properties.get_normal_quest_size(),
      properties.get_min_quest_size(),
      properties.get_max_quest_size()
  );
}

/**
 * \brief Enables accepting standard input lines as Lua commands.
 */
void MainLoop::initialize_lua_console() {

  // Watch stdin in a separate thread.
  stdin_thread = std::thread([this]() {

    std::string line;
    while (!is_exiting()) {

      // Note: don't enable the console on Windows when stdin is the default,
      // it continuously receives false positives and eats tons of CPU.
      // This should rather be used with a pipe typically from the editor.
      if (std::getline(std::cin, line)) {

        while (!line.empty() && std::isspace(line.at(line.size() - 1))) {
          line.erase(line.size() - 1);
        }

        if (!line.empty()) {
          push_lua_command(line);
        }
      }
    }
  });
  stdin_thread.detach();
}

/**
 * \brief Cleans resources started by initialize_lua_console().
 */
void MainLoop::quit_lua_console() {

  exiting = true;
  if (!stdin_thread.joinable()) {
      return;
  }

  stdin_thread.join();
}

/**
 * @brief create the root surface
 */
void MainLoop::make_root_surface() {
  Size s = Video::get_quest_size();
  switch (Video::get_geometry_mode()) {
  case Video::GeometryMode::DYNAMIC_ABSOLUTE:
  case Video::GeometryMode::DYNAMIC_QUEST_SIZE:
    s = Video::get_window_size();
    break;
  default:
    break;
  }
  if(!root_surface or root_surface->get_size() != s) {
    root_surface = Surface::create(s);
  }
}

}
