def rw(path, fn):
    s = open(path, newline='').read()
    crlf = '\r\n' in s
    if crlf:
        s = s.replace('\r\n', '\n')
    s = fn(s)
    if crlf:
        s = s.replace('\n', '\r\n')
    open(path, 'w', newline='').write(s)

ADD = '''
#ifdef __vita__
      // Virtual keyboard: the Vita has no keyboard, but many quests build
      // their menus around keys. Cross -> Return, Circle -> Escape.
      // These keys are not bound to game commands by default, so only
      // menus react to them. Create ux0:data/Solarus/nokeys.txt to disable.
      case SDL_CONTROLLERBUTTONDOWN:
      case SDL_CONTROLLERBUTTONUP:
      {
        static int keys_enabled = -1;
        if (keys_enabled < 0) {
          FILE* nk = fopen("ux0:data/Solarus/nokeys.txt", "r");
          keys_enabled = nk ? 0 : 1;
          if (nk) {
            fclose(nk);
          }
        }
        SDL_Keycode fake_key = 0;
        if (keys_enabled) {
          switch (internal_event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_A: fake_key = SDLK_RETURN; break;
            case SDL_CONTROLLER_BUTTON_B: fake_key = SDLK_ESCAPE; break;
            default: break;
          }
        }
        if (fake_key != 0) {
          const bool down = (internal_event.type == SDL_CONTROLLERBUTTONDOWN);
          SDL_Event key_event;
          SDL_zero(key_event);
          key_event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
          key_event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
          key_event.key.repeat = 0;
          key_event.key.keysym.sym = fake_key;
          key_event.key.keysym.scancode = SDL_GetScancodeFromKey(fake_key);
          key_event.key.keysym.mod = KMOD_NONE;
          SDL_PushEvent(&key_event);
        }
        break;
      }
#endif
'''

def fix(s):
    a = '''      case SDL_JOYBUTTONUP: {
        jbuttons_pressed.erase(internal_event.jbutton.button);
        break;
      }
'''
    assert s.count(a) == 1, "InputEvent: anchor"
    s = s.replace(a, a + ADD[1:])
    if '#include <cstdio>' not in s:
        s = s.replace('#include', '#include <cstdio>\n#include', 1)
    return s

rw('src/core/InputEvent.cpp', fix)
print("PATCH OK")
