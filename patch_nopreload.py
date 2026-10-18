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
    a = "  resource_provider.start_preloading_resources();\n"
    assert s.count(a) == 1, "ancre introuvable (deja patche ?)"
    return s.replace(a, "#ifndef __vita__\n  // On the Vita the memory is too small to preload every image.\n" + a + "#endif\n")

rw('src/core/MainLoop.cpp', fix)
print("NOPRELOAD_OK")
