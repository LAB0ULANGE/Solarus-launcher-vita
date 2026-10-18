import re
import subprocess
def rw(p, f):
    b = open(p, "rb").read()
    crlf = b"\r\n" in b
    s = b.decode("utf-8").replace("\r\n", "\n")
    s = f(s)
    if crlf: s = s.replace("\n", "\r\n")
    open(p, "wb").write(s.encode("utf-8"))

def ptrs(s):
    if "solarus_vita_live_tex" in s:
        print("SDLPtrs deja patche"); return s
    old = "SDL_DestroyTexture(texture);"
    assert s.count(old) == 1
    s = s.replace(old, "--solarus_vita_live_tex;\n      " + old, 1)
    inc = list(re.finditer(r"^#include[^\n]*\n", s, re.M))
    assert inc
    j = inc[-1].end()
    return s[:j] + '\nextern "C" int solarus_vita_live_tex;\n' + s[j:]

def sdl(s):
    if "solarus_vita_live_tex" in s:
        print("SDLSurfaceImpl deja patche"); return s
    a = "void (*solarus_vita_gc_hook)() = nullptr;\n"
    assert a in s
    s = s.replace(a, a + 'extern "C" { int solarus_vita_live_tex = 0; }\n', 1)
    old = '  SOLARUS_REQUIRE(tex != nullptr,\n      std::string("Failed to create render texture '
    assert old in s
    s = s.replace(old, "  if (tex != nullptr) { ++solarus_vita_live_tex; }\n" + old, 1)
    old2 = "  SOLARUS_REQUIRE(tex != nullptr,\n      std::string(\"Failed to convert surface to texture\")"
    assert old2 in s
    s = s.replace(old2, "  if (tex != nullptr) { ++solarus_vita_live_tex; }\n" + old2, 1)
    old3 = 'fprintf(texlog, "TEX target %dx%d %s | libre:'
    assert old3 in s
    s = s.replace(old3, 'fprintf(texlog, "TEX vivantes=%d target %dx%d %s | libre:', 1)
    old4 = 'width, height, tex ? "ok" : "FAILED", mi.size_user'
    assert old4 in s
    s = s.replace(old4, 'solarus_vita_live_tex, width, height, tex ? "ok" : "FAILED", mi.size_user', 1)
    return s

rw("include/solarus/graphics/SDLPtrs.h", ptrs)
rw("src/graphics/sdlrenderer/SDLSurfaceImpl.cpp", sdl)
print(subprocess.run("grep -rn 'SDL_CreateTexture' src | grep -v SDLSurfaceImpl.cpp", shell=True, capture_output=True, text=True).stdout or "autres creations: aucune")
print("PATCH_OK")
