def rw(p, f):
    b = open(p, "rb").read()
    crlf = b"\r\n" in b
    s = b.decode("utf-8").replace("\r\n", "\n")
    s = f(s)
    if crlf: s = s.replace("\n", "\r\n")
    open(p, "wb").write(s.encode("utf-8"))

def ptrs(s):
    if "solarus_vita_live_bytes" in s:
        print("SDLPtrs deja patche"); return s
    a = 'extern "C" int solarus_vita_live_tex;\n'
    assert a in s
    s = s.replace(a, a + 'extern "C" long long solarus_vita_live_bytes;\n', 1)
    b = "--solarus_vita_live_tex;\n"
    assert b in s
    s = s.replace(b, b + "      { int qw = 0, qh = 0; SDL_QueryTexture(texture, nullptr, nullptr, &qw, &qh); solarus_vita_live_bytes -= (long long)qw * qh * 4; }\n", 1)
    return s

def sdl(s):
    if "solarus_vita_live_bytes" in s:
        print("SDLSurfaceImpl deja patche"); return s
    a = 'extern "C" { int solarus_vita_live_tex = 0; }\n'
    assert a in s
    s = s.replace(a, 'extern "C" { int solarus_vita_live_tex = 0; long long solarus_vita_live_bytes = 0; }\n', 1)
    b = "  if (tex != nullptr) { ++solarus_vita_live_tex; }\n"
    assert s.count(b) == 2
    s = s.replace(b, "  if (tex != nullptr) { ++solarus_vita_live_tex; int qw = 0, qh = 0; SDL_QueryTexture(tex, nullptr, nullptr, &qw, &qh); solarus_vita_live_bytes += (long long)qw * qh * 4; }\n")
    c = '"TEX vivantes=%d target'
    assert c in s
    s = s.replace(c, '"TEX vivantes=%d (%dK) target', 1)
    d = "solarus_vita_live_tex, width, height"
    assert d in s
    s = s.replace(d, "solarus_vita_live_tex, (int)(solarus_vita_live_bytes / 1024), width, height", 1)
    return s

rw("include/solarus/graphics/SDLPtrs.h", ptrs)
rw("src/graphics/sdlrenderer/SDLSurfaceImpl.cpp", sdl)
print("PATCH_OK")
