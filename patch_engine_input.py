def rw(path, fn):
    s = open(path, newline='').read()
    crlf = '\r\n' in s
    if crlf:
        s = s.replace('\r\n', '\n')
    s = fn(s)
    if crlf:
        s = s.replace('\n', '\r\n')
    open(path, 'w', newline='').write(s)

LAYER = r'''
#ifdef __vita__
// ===== Vita engine input layer =====
// The engine, not the quest, decides what the pad does. Every pad event
// is removed from the event queue before any quest can see it and is
// converted into keyboard keys. Quests only ever see keys.
// Remap with ux0:data/Solarus/keymap.txt, lines like:  a=space  b=c  x=x
namespace {
SDL_Keycode vita_keymap[SDL_CONTROLLER_BUTTON_MAX];
std::vector<SDL_Event> vita_pending_keys;
int vita_stick_state[2] = { 0, 0 };
bool vita_layer_ready = false;

void vita_init_layer() {
  vita_layer_ready = true;
  for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) {
    vita_keymap[i] = SDLK_UNKNOWN;
  }
  vita_keymap[SDL_CONTROLLER_BUTTON_A] = SDLK_SPACE;
  vita_keymap[SDL_CONTROLLER_BUTTON_B] = SDLK_c;
  vita_keymap[SDL_CONTROLLER_BUTTON_X] = SDLK_x;
  vita_keymap[SDL_CONTROLLER_BUTTON_Y] = SDLK_v;
  vita_keymap[SDL_CONTROLLER_BUTTON_START] = SDLK_d;
  vita_keymap[SDL_CONTROLLER_BUTTON_BACK] = SDLK_ESCAPE;
  vita_keymap[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = SDLK_RETURN;
  vita_keymap[SDL_CONTROLLER_BUTTON_DPAD_UP] = SDLK_UP;
  vita_keymap[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = SDLK_DOWN;
  vita_keymap[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = SDLK_LEFT;
  vita_keymap[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = SDLK_RIGHT;

  FILE* f = fopen("ux0:data/Solarus/keymap.txt", "r");
  if (f) {
    char line[128];
    while (fgets(line, sizeof(line), f)) {
      char* eq = strchr(line, '=');
      if (!eq) {
        continue;
      }
      *eq = 0;
      char* value = eq + 1;
      for (char* p = value; *p; ++p) {
        if (*p == '\r' || *p == '\n') {
          *p = 0;
        }
      }
      SDL_GameControllerButton b = SDL_GameControllerGetButtonFromString(line);
      SDL_Keycode k = SDL_GetKeyFromName(value);
      if (b != SDL_CONTROLLER_BUTTON_INVALID && k != SDLK_UNKNOWN) {
        vita_keymap[b] = k;
      }
    }
    fclose(f);
  }
}

void vita_queue_key(SDL_Keycode key, bool down) {
  SDL_Event e;
  SDL_zero(e);
  e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
  e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
  e.key.repeat = 0;
  e.key.keysym.sym = key;
  e.key.keysym.scancode = SDL_GetScancodeFromKey(key);
  e.key.keysym.mod = KMOD_NONE;
  vita_pending_keys.push_back(e);
}

void vita_stick(int axis_index, Sint16 value, SDL_Keycode negative, SDL_Keycode positive) {
  int old_state = vita_stick_state[axis_index];
  int new_state = old_state;
  if (old_state == 0) {
    if (value < -16000) { new_state = -1; }
    else if (value > 16000) { new_state = 1; }
  }
  else if (old_state < 0 && value > -8000) {
    new_state = (value > 16000) ? 1 : 0;
  }
  else if (old_state > 0 && value < 8000) {
    new_state = (value < -16000) ? -1 : 0;
  }
  if (new_state == old_state) {
    return;
  }
  if (old_state < 0) { vita_queue_key(negative, false); }
  if (old_state > 0) { vita_queue_key(positive, false); }
  if (new_state < 0) { vita_queue_key(negative, true); }
  if (new_state > 0) { vita_queue_key(positive, true); }
  vita_stick_state[axis_index] = new_state;
}

int SDLCALL vita_queue_filter(void*, SDL_Event* e) {
  switch (e->type) {
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
      const int b = e->cbutton.button;
      if (b == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
        return 1;
      }
      if (b >= 0 && b < SDL_CONTROLLER_BUTTON_MAX && vita_keymap[b] != SDLK_UNKNOWN) {
        vita_queue_key(vita_keymap[b], e->type == SDL_CONTROLLERBUTTONDOWN);
      }
      return 0;
    }
    case SDL_CONTROLLERAXISMOTION:
      if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
        vita_stick(0, e->caxis.value, SDLK_LEFT, SDLK_RIGHT);
      }
      else if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
        vita_stick(1, e->caxis.value, SDLK_UP, SDLK_DOWN);
      }
      return 0;
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
    case SDL_JOYAXISMOTION:
    case SDL_JOYHATMOTION:
    case SDL_JOYBALLMOTION:
      return 0;
    default:
      return 1;
  }
}

void vita_pump_layer() {
  if (!vita_layer_ready) {
    vita_init_layer();
  }
  SDL_PumpEvents();
  SDL_FilterEvents(vita_queue_filter, nullptr);
  for (const SDL_Event& e : vita_pending_keys) {
    SDL_Event copy = e;
    SDL_PushEvent(&copy);
  }
  vita_pending_keys.clear();
}
}  // namespace
#endif

'''

def fix(s):
    if 'Vita engine input layer' in s:
        raise SystemExit("deja patche")
    sig = "std::unique_ptr<InputEvent> InputEvent::get_event() {\n"
    assert s.count(sig) == 1, "InputEvent: get_event anchor"
    s = s.replace(sig, sig + "#ifdef __vita__\n  vita_pump_layer();\n#endif\n")
    idx = s.index("std::unique_ptr<InputEvent> InputEvent::get_event() {")
    cidx = s.rfind("/**", 0, idx)
    s = s[:cidx] + LAYER.lstrip('\n') + "\n" + s[cidx:]
    for inc in ("<cstdio>", "<cstring>", "<vector>"):
        if "#include " + inc not in s:
            s = s.replace("#include", "#include " + inc + "\n#include", 1)
    return s

rw('src/core/InputEvent.cpp', fix)
print("PATCH OK")
