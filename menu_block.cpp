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
  if (!vita_is_launcher()) {
    items.push_back(1);
  }
  items.push_back(2);
  if (!vita_is_launcher()) {
    items.push_back(3);
  }
  return items;
}

void vita_menu_refresh() {
  std::vector<std::string> lines;
  const std::string cursor = "> ";
  const std::string none = "  ";
  if (vita_menu_mode == VITA_MENU_CLOSED) {
    solarus_vita_overlay_set(lines);
    return;
  }
  if (vita_menu_mode == VITA_MENU_MAIN) {
    lines.push_back("MENU DU MOTEUR");
    lines.push_back("");
    const std::vector<int> items = vita_main_items();
    for (size_t i = 0; i < items.size(); ++i) {
      const char* label = "";
      switch (items[i]) {
        case 0: label = vita_is_launcher() ? "REPRENDRE" : "REPRENDRE LA PARTIE"; break;
        case 1: label = "COMMANDES DE CE JEU"; break;
        case 2: label = "AFFICHAGE ETIRE OU CENTRE"; break;
        default: label = "RETOUR A LA LISTE DES JEUX"; break;
      }
      lines.push_back((vita_menu_sel == static_cast<int>(i) ? cursor : none) + label);
    }
    lines.push_back("");
    lines.push_back("~HAUT BAS CHOISIR   CROIX VALIDER   ROND RETOUR");
    lines.push_back("~SELECT + L OUVRE CE MENU EN JEU");
  }
  else if (vita_menu_mode == VITA_MENU_COMMANDS) {
    std::string id = vita_quest_id;
    if (id.size() > 28) {
      id = id.substr(0, 28);
    }
    lines.push_back("COMMANDES DE " + id);
    lines.push_back("");
    const std::vector<VitaKeyChoice>& choices = vita_key_choices();
    for (int i = 0; i < vita_num_pad_buttons; ++i) {
      lines.push_back((vita_menu_sel == i ? cursor : none) +
          vita_pad_name(vita_pad_buttons[i].label, 10) + ": " +
          choices[vita_choice_index(vita_pad_key[i])].label);
    }
    lines.push_back((vita_menu_sel == vita_num_pad_buttons ? cursor : none) + "REMETTRE LES COMMANDES PAR DEFAUT");
    lines.push_back((vita_menu_sel == vita_num_pad_buttons + 1 ? cursor : none) + "RETOUR");
    lines.push_back("");
    lines.push_back("~GAUCHE DROITE CHANGER LA TOUCHE   ROND RETOUR");
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
    case VITA_MENU_COMMANDS: return vita_num_pad_buttons + 2;
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
        Video::toggle_vita_stretch();
      }
      else {
        vita_menu_open(VITA_MENU_CONFIRM);
      }
      break;
    }
    case VITA_MENU_COMMANDS:
      if (vita_menu_sel < vita_num_pad_buttons) {
        vita_menu_change_key(vita_menu_sel, 1);
      }
      else if (vita_menu_sel == vita_num_pad_buttons) {
        vita_defaults();
        vita_save_map();
      }
      else {
        vita_menu_open(VITA_MENU_MAIN);
        vita_menu_sel = 1;
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
        vita_menu_sel = 3;
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
      vita_menu_sel = 1;
      break;
    case VITA_MENU_PROMPT:
      vita_save_map();
      vita_menu_close();
      break;
    case VITA_MENU_CONFIRM:
      vita_menu_open(VITA_MENU_MAIN);
      vita_menu_sel = 3;
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
      if (vita_menu_mode == VITA_MENU_COMMANDS && vita_menu_sel < vita_num_pad_buttons) {
        vita_menu_change_key(vita_menu_sel, -1);
      }
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      if (vita_menu_mode == VITA_MENU_COMMANDS && vita_menu_sel < vita_num_pad_buttons) {
        vita_menu_change_key(vita_menu_sel, 1);
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
  vita_ensure_map();
  if (!vita_is_launcher() && !vita_map_exists) {
    vita_menu_open(VITA_MENU_PROMPT);
  }
}

bool vita_menu_is_open() {
  return vita_menu_mode != VITA_MENU_CLOSED;
}
#endif

