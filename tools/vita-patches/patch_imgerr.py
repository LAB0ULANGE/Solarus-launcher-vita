def rw(path, fn):
    s = open(path, newline='').read()
    crlf = '\r\n' in s
    if crlf:
        s = s.replace('\r\n', '\n')
    s = fn(s)
    if crlf:
        s = s.replace('\n', '\r\n')
    open(path, 'w', newline='').write(s)

def fix(s):
    a = "      std::string(\"Cannot load image '\") + file_name + \"'\");"
    assert s.count(a) == 1, "ancre introuvable (deja patche ?)"
    b = ("      std::string(\"Cannot load image '\") + file_name + \"' (\" +\n"
         "      std::to_string(buffer.size()) + \" bytes): \" + SDL_GetError());")
    return s.replace(a, b)

rw('src/graphics/Surface.cpp', fix)
print("PATCH_OK")
