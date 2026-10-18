p = 'data/main.lua'
s = open(p, newline='', encoding='utf-8').read()
crlf = '\r\n' in s
s = s.replace('\r\n', '\n')
def rep(old, new):
    global s
    assert s.count(old) == 1, "ancre introuvable : " + old[:40]
    s = s.replace(old, new)
rep('local function handle_input(name)\n',
    'local last_move = -1000\n\nlocal function handle_input(name)\n')
rep('    if name == "b" or name == "escape" then\n      sol.main.vita_download_cancel()',
    '    if name == "b" or name == "c" or name == "escape" then\n      sol.main.vita_download_cancel()')
rep('  if name == "dpad_up" or name == "up" then\n',
    '  if name == "dpad_up" or name == "up" or name == "dpad_down" or name == "down"\n      or name == "dpad_left" or name == "left" or name == "dpad_right" or name == "right" then\n'
    '    local now = sol.main.get_elapsed_time()\n    if now - last_move < 150 then\n      return true\n    end\n    last_move = now\n  end\n'
    '  if name == "dpad_up" or name == "up" then\n')
rep('  elseif name == "b" or name == "escape" then\n    sol.main.exit()\n  else\n',
    '  else\n')
rep('    O: quit', '')
if crlf: s = s.replace('\n', '\r\n')
open(p, 'w', newline='', encoding='utf-8').write(s)
print("PATCH OK")
