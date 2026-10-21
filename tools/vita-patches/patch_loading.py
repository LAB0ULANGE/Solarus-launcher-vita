# Ecran de chargement : moteur (C++) + launcher (Lua).
import os

def rw(path, fn):
    s = open(path, newline='').read()
    crlf = '\r\n' in s
    if crlf:
        s = s.replace('\r\n', '\n')
    s = fn(s)
    if crlf:
        s = s.replace('\n', '\r\n')
    open(path, 'w', newline='').write(s)

def once(s, a, b, name):
    assert s.count(a) == 1, "ancre introuvable: " + name
    return s.replace(a, b)

def fix_main(s):
    assert "vita_show_loading" not in s, "MainLoop.cpp deja patche"
    a = "void vita_first_step_check();\nbool vita_menu_is_open();\n#endif\n"
    helper = (
        "void vita_first_step_check();\nbool vita_menu_is_open();\n"
        "void solarus_vita_overlay_set(const std::vector<std::string>& lines);\n"
        "void solarus_vita_overlay_opaque(bool opaque);\n"
        "\n"
        "// Loading screen: drawn once as soon as the video is ready, and kept on\n"
        "// screen until the first step of the main loop.\n"
        "static void vita_show_loading() {\n"
        "  std::vector<std::string> lines;\n"
        "  lines.push_back(\"SOLARUS\");\n"
        "  lines.push_back(\"\");\n"
        "  lines.push_back(\"  CHARGEMENT...\");\n"
        "  if (!vita_quest_id.empty() && vita_quest_id != \"launcher\") {\n"
        "    std::string n = vita_quest_id;\n"
        "    if (n.size() > 30) {\n"
        "      n = n.substr(0, 30);\n"
        "    }\n"
        "    lines.push_back(\"\");\n"
        "    lines.push_back(\"~\" + n);\n"
        "  }\n"
        "  solarus_vita_overlay_opaque(true);\n"
        "  solarus_vita_overlay_set(lines);\n"
        "  Video::finish();\n"
        "}\n"
        "\n"
        "static void vita_hide_loading() {\n"
        "  solarus_vita_overlay_opaque(false);\n"
        "  solarus_vita_overlay_set(std::vector<std::string>());\n"
        "}\n"
        "#endif\n")
    s = once(s, a, helper, "declarations")
    a = ("  // Initialize engine features (audio, video...).\n"
         "  System::initialize(args, resource_provider);\n")
    s = once(s, a, a + "#ifdef __vita__\n  vita_show_loading();\n#endif\n", "System::initialize")
    a = "void vita_first_step_check() {\n  static bool done = false;\n  if (done) {\n    return;\n  }\n  done = true;\n"
    s = once(s, a, a + "  vita_hide_loading();\n", "first step")
    return s

def fix_sdl(s):
    assert "vita_overlay_opaque" not in s, "SDLRenderer.cpp deja patche"
    a = "static std::vector<std::string> vita_overlay_lines;\n"
    s = once(s, a, a + "static bool vita_overlay_opaque = false;\n"
        "void solarus_vita_overlay_opaque(bool opaque) {\n  vita_overlay_opaque = opaque;\n}\n", "flag")
    a = "    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);\n    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 215);\n"
    s = once(s, a,
        "    if (vita_overlay_opaque) {\n"
        "      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);\n"
        "      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);\n"
        "      SDL_RenderClear(renderer);\n"
        "    }\n" + a, "clear")
    return s

def fix_lua(s):
    assert 'state = "launching"' not in s, "main.lua deja patche"
    a = ("local function start_game(game)\n"
         "  local ok = sol.main.vita_launch(QUESTS_DIR .. game.file)\n"
         "  if not ok then\n"
         "    state = \"error\"\n"
         "    message = \"Could not start the game.\"\n"
         "  end\n"
         "end\n")
    b = ("local function start_game(game)\n"
         "  state = \"launching\"\n"
         "  message = \"Chargement de \" .. game.name .. \"...\"\n"
         "  -- Let one frame be drawn before the app restarts.\n"
         "  sol.timer.start(sol.main, 200, function()\n"
         "    local ok = sol.main.vita_launch(QUESTS_DIR .. game.file)\n"
         "    if not ok then\n"
         "      state = \"error\"\n"
         "      message = \"Could not start the game.\"\n"
         "    end\n"
         "  end)\n"
         "end\n")
    s = once(s, a, b, "start_game")
    a = "local function handle_input(name)\n  if state == \"loading\" then\n"
    s = once(s, a, "local function handle_input(name)\n  if state == \"loading\" or state == \"launching\" then\n", "input")
    a = "  background:draw(dst, 0, 0)\n  title:draw(dst, 16, 10)\n"
    s = once(s, a, a + "  if state == \"launching\" then\n    status:set_text(message)\n    status:draw(dst, 16, 120)\n    hint:set_text(\"\")\n    hint:draw(dst, 16, 252)\n    return\n  end\n", "draw")
    return s

rw('src/core/MainLoop.cpp', fix_main)
rw('src/graphics/sdlrenderer/SDLRenderer.cpp', fix_sdl)
lua = os.path.expanduser('~/vpk_engine/data/main.lua') if os.path.exists(os.path.expanduser('~/vpk_engine/data/main.lua')) else 'vpk_engine/data/main.lua'
rw(lua, fix_lua)
print("LOADING_OK")
