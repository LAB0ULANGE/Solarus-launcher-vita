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

rw('data/main.lua', fix_lua)
print("LUA_OK")
