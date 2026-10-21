def rw(path, fn):
    s = open(path, newline='').read()
    crlf = '\r\n' in s
    if crlf:
        s = s.replace('\r\n', '\n')
    s = fn(s)
    if crlf:
        s = s.replace('\n', '\r\n')
    open(path, 'w', newline='').write(s)

def fix_input(s):
    old = "            case SDL_CONTROLLER_BUTTON_B: fake_key = SDLK_ESCAPE; break;\n"
    new = old + (
      "            case SDL_CONTROLLER_BUTTON_DPAD_UP: fake_key = SDLK_UP; break;\n"
      "            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: fake_key = SDLK_DOWN; break;\n"
      "            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: fake_key = SDLK_LEFT; break;\n"
      "            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: fake_key = SDLK_RIGHT; break;\n")
    assert s.count(old) == 1, "InputEvent: anchor"
    assert "SDL_CONTROLLER_BUTTON_DPAD_UP: fake_key" not in s, "deja patche"
    return s.replace(old, new)

def fix_lua(s):
    old = "if (event.get_dpad_direction() != -2) {"
    assert s.count(old) == 1, "LuaContext: anchor"
    return s.replace(old, "if (false && event.get_dpad_direction() != -2) {")

rw('src/core/InputEvent.cpp', fix_input)
rw('src/lua/LuaContext.cpp', fix_lua)
print("PATCH OK")
