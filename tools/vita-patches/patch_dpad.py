p = "src/core/MainLoop.cpp"
b = open(p, "rb").read(); crlf = b"\r\n" in b
s = b.decode("utf-8").replace("\r\n", "\n")
old = "  if (event.is_joypad_event() && event.get_dpad_direction() != -2) {\n"
new = "  if (!solarus_vita_pad_mode_active() && event.is_joypad_event() && event.get_dpad_direction() != -2) {\n"
if new in s:
    print("deja patche")
else:
    assert s.count(old) == 1 and "bool solarus_vita_pad_mode_active() {" in s, "motif introuvable (patch_padnames applique ?)"
    s = s.replace(old, new, 1)
    if crlf: s = s.replace("\n", "\r\n")
    open(p, "wb").write(s.encode("utf-8")); print("PATCH_OK")
