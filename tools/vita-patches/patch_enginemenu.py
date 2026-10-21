# Patch: menu du moteur (Select) + commandes par jeu + invite au premier lancement.
# Usage (depuis ~/solarus-vita-v2) : python3 patch_enginemenu.py menu_block.cpp
import sys, os

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

block = open(sys.argv[1]).read().replace('\r\n', '\n')
root = sys.argv[2] if len(sys.argv) > 2 else "."
P_MAIN = os.path.join(root, "src/core/MainLoop.cpp")
P_INP = os.path.join(root, "src/core/InputEvent.cpp")
P_SDL = os.path.join(root, "src/graphics/sdlrenderer/SDLRenderer.cpp")

def fix_main(s):
    assert "vita_menu_event" not in s, "MainLoop.cpp deja patche"
    s = once(s, "#include <psp2/kernel/clib.h>\n#include <cstdio>\n#endif\n",
        "#include <psp2/kernel/clib.h>\n#include <cstdio>\n#endif\n"
        "#ifdef __vita__\n#include <psp2/appmgr.h>\n#include <psp2/io/stat.h>\n"
        "#include <set>\n#include <vector>\n#include <cstring>\n#endif\n", "includes")
    a = "namespace Solarus {\n\nnamespace {\n\n/**\n * \\brief Checks that the quest is compatible"
    s = once(s, a, "namespace Solarus {\n\n#ifdef __vita__\nstd::string vita_quest_id;\n"
        "void vita_first_step_check();\nbool vita_menu_is_open();\n#endif\n\n"
        + a[len("namespace Solarus {\n\n"):], "namespace")
    a = "    if (!vita_quest.empty()) {\n      return vita_quest;\n"
    ident = (
        "    if (bundled && !play_mode) {\n"
        "      vita_quest_id = \"launcher\";\n"
        "    }\n"
        "    else {\n"
        "      std::string n = vita_quest;\n"
        "      while (!n.empty() && (n.back() == '/' || n.back() == '\\\\')) {\n"
        "        n.pop_back();\n"
        "      }\n"
        "      size_t slash = n.find_last_of(\"/\\\\:\");\n"
        "      if (slash != std::string::npos) {\n"
        "        n = n.substr(slash + 1);\n"
        "      }\n"
        "      size_t dot = n.rfind('.');\n"
        "      if (dot != std::string::npos && dot > 0) {\n"
        "        n = n.substr(0, dot);\n"
        "      }\n"
        "      vita_quest_id = n.empty() ? std::string(\"game\") : n;\n"
        "    }\n")
    s = once(s, a, ident + a, "quest id")
    a = "void solarus_vita_toggle_legend(const std::string& text);\n#endif\n\nvoid MainLoop::notify_input"
    s = once(s, a, "void solarus_vita_toggle_legend(const std::string& text);\n#endif\n\n"
        + block + "\nvoid MainLoop::notify_input", "block")
    a = "void MainLoop::step(uint64_t timestep_ns) {\n  SOL_PFUN();\n"
    s = once(s, a, a + "#ifdef __vita__\n  vita_first_step_check();\n  if (vita_menu_is_open()) {\n    return;  // Engine menu open: the game is frozen.\n  }\n#endif\n", "step")
    a = "void MainLoop::notify_input(const InputEvent& event) {\n#ifdef __vita__\n"
    s = once(s, a, a + "  if (vita_menu_event(event)) {\n    return;\n  }\n", "menu hook")
    a = "    return;\n  }\n#endif\n\n  bool handled = false;\n"
    s = once(s, a, "    return;\n  }\n  // Every other pad button and the left stick become keys, using the\n"
        "  // mapping chosen for this game in the engine menu.\n"
        "  if (vita_pad_event(event)) {\n    return;\n  }\n#endif\n\n  bool handled = false;\n", "pad hook")
    # Old Select legend: disabled (Select is a normal game button now).
    a = "  if (event.is_joypad_button_pressed() &&\n      event.get_joypad_button() == JoyPadButton::BACK) {"
    s = once(s, a, "  if (false && event.is_joypad_button_pressed() &&\n      event.get_joypad_button() == JoyPadButton::BACK) {", "old select")
    return s

def fix_inp(s):
    a = "keys_enabled = nk ? 0 : 1;"
    if a in s:
        s = s.replace(a, "keys_enabled = 0;  // the engine converts the pad itself (MainLoop)")
    else:
        assert "keys_enabled = 0;" in s, "InputEvent.cpp: bloc clavier virtuel introuvable"
    return s

def fix_sdl(s):
    assert "vita_overlay_lines" not in s, "SDLRenderer.cpp deja patche"
    if "#include <vector>" not in s:
        s = once(s, "#include <string>\n", "#include <string>\n#include <vector>\n", "sdl include")
    s = once(s, 'static const char vita_font_chars[] = " 0123456789=?ABCDEFGHIJKLMNOPQRSTUVWXYZ";',
             'static const char vita_font_chars[] = " 0123456789=?ABCDEFGHIJKLMNOPQRSTUVWXYZ.:-><!/+()_";', "chars")
    s = once(s, "vita_font[39][5] = {", "vita_font[50][5] = {", "size")
    s = once(s, "{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}\n};",
        "{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},\n"
        "  {0x00,0x60,0x60,0x00,0x00},{0x00,0x36,0x36,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},\n"
        "  {0x41,0x22,0x14,0x08,0x00},{0x08,0x14,0x22,0x41,0x00},{0x00,0x00,0x5F,0x00,0x00},\n"
        "  {0x20,0x10,0x08,0x04,0x02},{0x08,0x08,0x3E,0x08,0x08},{0x00,0x1C,0x22,0x41,0x00},\n"
        "  {0x00,0x41,0x22,0x1C,0x00},{0x40,0x40,0x40,0x40,0x40}\n};\n"
        "\n"
        "static std::vector<std::string> vita_overlay_lines;\n"
        "void solarus_vita_overlay_set(const std::vector<std::string>& lines) {\n"
        "  vita_overlay_lines = lines;\n"
        "}\n"
        "\n"
        "static void vita_draw_text(SDL_Renderer* r, const std::string& text, int x, int y, int scale) {\n"
        "  for (char c : text) {\n"
        "    if (c >= 'a' && c <= 'z') {\n"
        "      c = static_cast<char>(c - 'a' + 'A');\n"
        "    }\n"
        "    int index = 12;  // '?' by default\n"
        "    for (int i = 0; vita_font_chars[i] != 0; ++i) {\n"
        "      if (vita_font_chars[i] == c) {\n"
        "        index = i;\n"
        "        break;\n"
        "      }\n"
        "    }\n"
        "    for (int col = 0; col < 5; ++col) {\n"
        "      for (int row = 0; row < 7; ++row) {\n"
        "        if (vita_font[index][col] & (1 << row)) {\n"
        "          SDL_Rect px = { x + col * scale, y + row * scale, scale, scale };\n"
        "          SDL_RenderFillRect(r, &px);\n"
        "        }\n"
        "      }\n"
        "    }\n"
        "    x += 6 * scale;\n"
        "  }\n"
        "}\n", "glyphs")
    a = "#ifdef __vita__\n  if (vita_legend_visible) {\n    int w = 960, h = 544;"
    overlay = (
        "#ifdef __vita__\n"
        "  if (!vita_overlay_lines.empty()) {\n"
        "    int w = 960, h = 544;\n"
        "    SDL_GetRendererOutputSize(renderer, &w, &h);\n"
        "    SDL_Rect old_viewport;\n"
        "    SDL_RenderGetViewport(renderer, &old_viewport);\n"
        "    float old_sx = 1.0f, old_sy = 1.0f;\n"
        "    SDL_RenderGetScale(renderer, &old_sx, &old_sy);\n"
        "    SDL_Rect old_clip;\n"
        "    SDL_bool clip_enabled = SDL_RenderIsClipEnabled(renderer);\n"
        "    SDL_RenderGetClipRect(renderer, &old_clip);\n"
        "    Uint8 or_ = 0, og = 0, ob = 0, oa = 0;\n"
        "    SDL_GetRenderDrawColor(renderer, &or_, &og, &ob, &oa);\n"
        "    SDL_BlendMode old_blend;\n"
        "    SDL_GetRenderDrawBlendMode(renderer, &old_blend);\n"
        "    SDL_RenderSetClipRect(renderer, nullptr);\n"
        "    SDL_RenderSetViewport(renderer, nullptr);\n"
        "    SDL_RenderSetScale(renderer, 1.0f, 1.0f);\n"
        "\n"
        "    int maxlen = 0;\n"
        "    for (const std::string& l : vita_overlay_lines) {\n"
        "      int n = static_cast<int>(l.size());\n"
        "      if (!l.empty() && l[0] == '~') {\n"
        "        --n;\n"
        "      }\n"
        "      if (n > maxlen) {\n"
        "        maxlen = n;\n"
        "      }\n"
        "    }\n"
        "    const int scale = (maxlen * 12 + 60 <= w) ? 2 : 1;\n"
        "    const int cw = 6 * scale;\n"
        "    const int line_h = 7 * scale + 3 * scale;\n"
        "    const int pw = maxlen * cw + 40;\n"
        "    const int ph = static_cast<int>(vita_overlay_lines.size()) * line_h + 30;\n"
        "    const int px0 = (w - pw) / 2;\n"
        "    const int py0 = (h - ph) / 2;\n"
        "    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);\n"
        "    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 215);\n"
        "    SDL_Rect panel = { px0, py0, pw, ph };\n"
        "    SDL_RenderFillRect(renderer, &panel);\n"
        "    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);\n"
        "    SDL_RenderDrawRect(renderer, &panel);\n"
        "    int y = py0 + 15;\n"
        "    for (size_t i = 0; i < vita_overlay_lines.size(); ++i) {\n"
        "      std::string l = vita_overlay_lines[i];\n"
        "      if (!l.empty() && l[0] == '~') {\n"
        "        l = l.substr(1);\n"
        "        SDL_SetRenderDrawColor(renderer, 170, 170, 170, 255);\n"
        "      }\n"
        "      else if (i == 0) {\n"
        "        SDL_SetRenderDrawColor(renderer, 90, 220, 255, 255);\n"
        "      }\n"
        "      else if (!l.empty() && l[0] == '>') {\n"
        "        SDL_SetRenderDrawColor(renderer, 255, 220, 60, 255);\n"
        "      }\n"
        "      else {\n"
        "        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);\n"
        "      }\n"
        "      vita_draw_text(renderer, l, px0 + 20, y, scale);\n"
        "      y += line_h;\n"
        "    }\n"
        "\n"
        "    SDL_SetRenderDrawColor(renderer, or_, og, ob, oa);\n"
        "    SDL_SetRenderDrawBlendMode(renderer, old_blend);\n"
        "    SDL_RenderSetViewport(renderer, &old_viewport);\n"
        "    SDL_RenderSetScale(renderer, old_sx, old_sy);\n"
        "    SDL_RenderSetClipRect(renderer, clip_enabled ? &old_clip : nullptr);\n"
        "  }\n"
        "#endif\n")
    s = once(s, a, overlay + a, "overlay")
    return s

rw(P_MAIN, fix_main)
rw(P_INP, fix_inp)
rw(P_SDL, fix_sdl)
print("PATCH_OK")
