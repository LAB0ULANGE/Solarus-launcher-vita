p = "src/core/MainLoop.cpp"
b = open(p, "rb").read()
crlf = b"\r\n" in b
s = b.decode("utf-8").replace("\r\n", "\n")
if "vita_apply_layout" in s:
    print("deja patche")
    raise SystemExit
assert "bool solarus_vita_pad_mode_active() {" in s, "applique d'abord patch_padnames.py"
R = []
R.append(("""void vita_first_step_check();
bool vita_menu_is_open();
""", """void vita_first_step_check();
bool vita_menu_is_open();
MainLoop* vita_main_loop = nullptr;
void solarus_vita_layout_start();
void solarus_vita_layout_step();
"""))
R.append(("""bool vita_pad_mode = true;  // true: the game receives the Vita buttons itself.
""", """bool vita_pad_mode = true;  // true: the game receives the Vita buttons itself.
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
"""))
R.append(("""    lines.push_back((vita_menu_sel == n - 2 ? cursor : none) + "REMETTRE LES COMMANDES PAR DEFAUT");
""", """    lines.push_back((vita_menu_sel == n - 2 ? cursor : none) +
        std::string(vita_pad_mode ? "DISPOSITION VITA : CROIX ACTION" : "REMETTRE LES COMMANDES PAR DEFAUT"));
    if (vita_pad_mode) {
      lines.push_back("~ROND EPEE   CARRE ET TRIANGLE OBJETS");
    }
"""))
R.append(("""      else if (vita_menu_sel == vita_cmd_count() - 2) {
        vita_defaults();
        vita_save_map();
      }
""", """      else if (vita_menu_sel == vita_cmd_count() - 2) {
        if (vita_pad_mode) {
          vita_apply_layout();
          vita_menu_close();
          return;
        }
        vita_defaults();
        vita_save_map();
      }
"""))
R.append(("""bool solarus_vita_pad_mode_active() {
  vita_ensure_map();
  return vita_pad_mode && !vita_is_launcher();
}
""", """bool solarus_vita_pad_mode_active() {
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
"""))
R.append(("""  vita_gc_ctx = lua_context.get();
""", """  vita_gc_ctx = lua_context.get();
  vita_main_loop = this;
"""))
R.append(("""    if (game != nullptr) {
      game->start();
#ifdef __vita__
      vita_lock_ps(false);
#endif
""", """    if (game != nullptr) {
      game->start();
#ifdef __vita__
      vita_lock_ps(false);
      solarus_vita_layout_start();
#endif
"""))
R.append(("""  if (game != nullptr) {
    game->update();
  }
  lua_context->update();
""", """  if (game != nullptr) {
    game->update();
  }
#ifdef __vita__
  solarus_vita_layout_step();
#endif
  lua_context->update();
"""))
for old, new in R:
    assert s.count(old) == 1, "motif introuvable : " + old[:70]
    s = s.replace(old, new, 1)
if crlf: s = s.replace("\n", "\r\n")
open(p, "wb").write(s.encode("utf-8"))
print("PATCH_OK")
