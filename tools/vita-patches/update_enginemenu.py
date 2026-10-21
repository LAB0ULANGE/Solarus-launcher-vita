# Mise a jour du menu moteur deja installe (Select+L, R = affichage).
import sys

def rw(path, fn):
    s = open(path, newline='').read()
    crlf = '\r\n' in s
    if crlf:
        s = s.replace('\r\n', '\n')
    s = fn(s)
    if crlf:
        s = s.replace('\n', '\r\n')
    open(path, 'w', newline='').write(s)

block = open(sys.argv[1]).read().replace('\r\n', '\n')
root = sys.argv[2] if len(sys.argv) > 2 else "."

def fix_main(s):
    start_mark = "#ifdef __vita__\nvoid solarus_vita_overlay_set(const std::vector<std::string>& lines);\n"
    assert s.count(start_mark) == 1, "ancien bloc menu introuvable"
    a = s.index(start_mark)
    b = s.index("void MainLoop::notify_input(const InputEvent& event) {")
    assert a < b
    s = s[:a] + block + "\n" + s[b:]
    # R : redevient l'affichage etire/centre
    r_off = "  if (false && event.is_joypad_button_pressed() &&\n      event.get_joypad_button() == JoyPadButton::RIGHT_SHOULDER) {"
    r_on = "  if (event.is_joypad_button_pressed() &&\n      event.get_joypad_button() == JoyPadButton::RIGHT_SHOULDER) {"
    s = s.replace(r_off, r_on)
    assert r_on in s, "ancre R introuvable"
    # Select : l'ancienne legende est desactivee
    b_on = "  if (event.is_joypad_button_pressed() &&\n      event.get_joypad_button() == JoyPadButton::BACK) {"
    b_off = "  if (false && event.is_joypad_button_pressed() &&\n      event.get_joypad_button() == JoyPadButton::BACK) {"
    if b_on in s:
        s = s.replace(b_on, b_off)
    assert b_off in s, "ancre Select introuvable"
    return s

rw(root + "/src/core/MainLoop.cpp", fix_main)
print("UPDATE_OK")
