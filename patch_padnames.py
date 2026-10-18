def rw(p, f):
    b = open(p, "rb").read()
    crlf = b"\r\n" in b
    s = b.decode("utf-8").replace("\r\n", "\n")
    s2 = f(s)
    if s2 is None:
        return
    if crlf: s2 = s2.replace("\n", "\r\n")
    open(p, "wb").write(s2.encode("utf-8"))
    print(p, ": OK")

def rep(s, old, new):
    assert s.count(old) == 1, "motif introuvable : " + old[:70]
    return s.replace(old, new, 1)

def mainloop(s):
    if "solarus_vita_pad_mode_active" in s:
        print("MainLoop deja patche"); return None
    return rep(s, """bool vita_menu_is_open() {
  return vita_menu_mode != VITA_MENU_CLOSED;
}
""", """bool vita_menu_is_open() {
  return vita_menu_mode != VITA_MENU_CLOSED;
}

// True when the current game receives the Vita buttons itself (pad mode).
bool solarus_vita_pad_mode_active() {
  vita_ensure_map();
  return vita_pad_mode && !vita_is_launcher();
}
""")

def gameapi(s):
    if "vita_joypad_display" in s:
        print("GameApi deja patche"); return None
    s = rep(s, """const std::string LuaContext::game_module_name = "sol.game";
""", """const std::string LuaContext::game_module_name = "sol.game";

#ifdef __vita__
// VITA_PAD_NAMES: the quests show joypad bindings with Xbox names (a, b...).
// On the Vita we show the real buttons instead, and translate them back.
bool solarus_vita_pad_mode_active();
namespace {
const char* const vita_joypad_names[][2] = {
  { "a", "croix" },
  { "b", "rond" },
  { "x", "carre" },
  { "y", "triangle" },
  { "back", "select" },
  { "left_shoulder", "L" },
  { "right_shoulder", "R" },
  { "dpad_up", "haut" },
  { "dpad_down", "bas" },
  { "dpad_left", "gauche" },
  { "dpad_right", "droite" },
};
std::string vita_joypad_display(const std::string& name) {
  for (const auto& n : vita_joypad_names) {
    if (name == n[0]) {
      return n[1];
    }
  }
  return name;
}
std::string vita_joypad_internal(const std::string& name) {
  for (const auto& n : vita_joypad_names) {
    if (name == n[1]) {
      return n[0];
    }
  }
  return name;
}
}
#endif
""")
    s = rep(s, """    const Controls& commands = game->get_controls();
    InputEvent::KeyboardKey key = commands.get_keyboard_binding(command);
    const std::string& key_name = enum_to_name(key);
""", """#ifdef __vita__
    if (solarus_vita_pad_mode_active()) {
      // Pad mode: the keyboard is not used, show nothing.
      push_string(l, std::string());
      return 1;
    }
#endif
    const Controls& commands = game->get_controls();
    InputEvent::KeyboardKey key = commands.get_keyboard_binding(command);
    const std::string& key_name = enum_to_name(key);
""")
    s = rep(s, """      push_string(l, binding->to_string());
""", """      std::string joypad_name = binding->to_string();
#ifdef __vita__
      joypad_name = vita_joypad_display(joypad_name);
#endif
      push_string(l, joypad_name);
""")
    s = rep(s, """    const std::string& joypad_string = LuaTools::opt_string(l, 3, "");
""", """    std::string joypad_string = LuaTools::opt_string(l, 3, "");
#ifdef __vita__
    joypad_string = vita_joypad_internal(joypad_string);
#endif
""")
    return s

rw("src/core/MainLoop.cpp", mainloop)
rw("src/lua/GameApi.cpp", gameapi)
print("PATCH_OK")
