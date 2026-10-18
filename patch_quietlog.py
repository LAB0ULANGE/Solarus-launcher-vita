def rw(p, pairs):
    b = open(p, "rb").read()
    crlf = b"\r\n" in b
    s = b.decode("utf-8").replace("\r\n", "\n")
    n = 0
    for old, new in pairs:
        if new in s:
            continue
        assert old in s, p + " : motif introuvable : " + old[:60]
        s = s.replace(old, new, 1)
        n += 1
    if crlf: s = s.replace("\n", "\r\n")
    open(p, "wb").write(s.encode("utf-8"))
    print(p, ":", n, "modif(s)")

rw("src/graphics/sdlrenderer/SDLSurfaceImpl.cpp", [(
    'FILE* texlog = fopen("ux0:data/solarus_debug.log", "a");',
    'FILE* texlog = (tex != nullptr) ? nullptr : fopen("ux0:data/solarus_debug.log", "a");')])
rw("cli/src/main.cpp", [(
    '  FILE* f = fopen("ux0:data/solarus_debug.log", "a");\n  if (f) {\n    fprintf(f, "RT new',
    '  FILE* f = (err >= 0) ? nullptr : fopen("ux0:data/solarus_debug.log", "a");\n  if (f) {\n    fprintf(f, "RT new')])
rw("src/core/MainLoop.cpp", [(
    "SDL_LogSetAllPriority(SDL_LOG_PRIORITY_WARN);",
    "SDL_LogSetAllPriority(SDL_LOG_PRIORITY_ERROR);")])
print("PATCH_OK")
