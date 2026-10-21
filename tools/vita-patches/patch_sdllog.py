p = "src/core/MainLoop.cpp"
b = open(p, "rb").read()
crlf = b"\r\n" in b
s = b.decode("utf-8").replace("\r\n", "\n")
if "vita_sdl_log_cb" in s:
    print("deja patche")
else:
    a = "static void vita_gc_now() {\n"
    assert a in s
    s = s.replace(a, """static void vita_sdl_log_cb(void*, int category, SDL_LogPriority priority, const char* message) {
  FILE* f = fopen("ux0:data/solarus_debug.log", "a");
  if (f) { fprintf(f, "SDL[%d/%d] %s\\n", category, (int)priority, message); fclose(f); }
}
""" + a, 1)
    b2 = "  solarus_vita_gc_hook = &vita_gc_now;\n"
    assert b2 in s
    s = s.replace(b2, b2 + "  SDL_LogSetOutputFunction(&vita_sdl_log_cb, nullptr);\n  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_WARN);\n", 1)
    c = "#include <lua.hpp>\n"
    assert c in s
    s = s.replace(c, c + "#include <SDL_log.h>\n#include <cstdio>\n", 1)
    if crlf: s = s.replace("\n", "\r\n")
    open(p, "wb").write(s.encode("utf-8"))
    print("PATCH_OK")
