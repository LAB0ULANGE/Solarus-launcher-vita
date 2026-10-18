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
    for name in ("UP", "DOWN", "LEFT", "RIGHT"):
        line = "            case SDL_CONTROLLER_BUTTON_DPAD_%s: fake_key = SDLK_%s; break;\n" % (name, name)
        assert s.count(line) == 1, "InputEvent: ligne croix " + name
        s = s.replace(line, "")
    return s

def fix_lua(s):
    a = "if (false && event.get_dpad_direction() != -2) {"
    assert s.count(a) == 1, "LuaContext: ancre 1"
    s = s.replace(a, "if (event.get_dpad_direction() != -2) {")
    b = ("      else if (event.is_joypad_button_pressed()) {\n"
         "        handled = on_joypad_button_pressed(event) || handled;\n"
         "      }\n"
         "      else {\n"
         "        handled = on_joypad_button_released(event) || handled;\n"
         "      }\n"
         "    }\n"
         "    else\n"
         "#endif\n")
    assert s.count(b) == 1, "LuaContext: ancre 2"
    return s.replace(b, "    }\n    else\n#endif\n")

def fix_main(s):
    a = ("    solarus_vita_toggle_legend(text);\n"
         "    return;\n"
         "  }\n"
         "#endif\n")
    assert s.count(a) == 1, "MainLoop: ancre"
    new = ("    solarus_vita_toggle_legend(text);\n"
           "    return;\n"
           "  }\n"
           "  // D-pad: the engine decides. A screen that has an on_joypad_hat_moved()\n"
           "  // handler receives it; for every other screen the engine sends the\n"
           "  // matching arrow key, once.\n"
           "  if (event.is_joypad_event() && event.get_dpad_direction() != -2) {\n"
           "    if (!lua_context->notify_input(event)) {\n"
           "      const int d = event.get_dpad_direction();\n"
           "      const bool down = event.is_joypad_button_pressed();\n"
           "      SDL_Keycode k = (d == 0) ? SDLK_RIGHT : (d == 2) ? SDLK_UP : (d == 4) ? SDLK_LEFT : SDLK_DOWN;\n"
           "      SDL_Event ke;\n"
           "      SDL_zero(ke);\n"
           "      ke.type = down ? SDL_KEYDOWN : SDL_KEYUP;\n"
           "      ke.key.state = down ? SDL_PRESSED : SDL_RELEASED;\n"
           "      ke.key.repeat = 0;\n"
           "      ke.key.keysym.sym = k;\n"
           "      ke.key.keysym.scancode = SDL_GetScancodeFromKey(k);\n"
           "      ke.key.keysym.mod = KMOD_NONE;\n"
           "      SDL_PushEvent(&ke);\n"
           "    }\n"
           "    return;\n"
           "  }\n"
           "#endif\n")
    return s.replace(a, new)

rw('src/core/InputEvent.cpp', fix_input)
rw('src/lua/LuaContext.cpp', fix_lua)
rw('src/core/MainLoop.cpp', fix_main)
print("PATCH OK")
