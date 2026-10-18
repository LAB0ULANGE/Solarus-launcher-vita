import sys
root = sys.argv[1] if len(sys.argv) > 1 else "."
def rw(p, f):
    b = open(p, "rb").read()
    crlf = b"\r\n" in b
    s = b.decode("utf-8").replace("\r\n", "\n")
    s = f(s)
    if crlf: s = s.replace("\n", "\r\n")
    open(p, "wb").write(s.encode("utf-8"))

def sdl(s):
    if "solarus_vita_gc_hook" in s:
        print("SDLSurfaceImpl deja patche"); return s
    a = "SDL_Texture* create_texture_from_renderer("
    assert a in s
    s = s.replace(a, "void (*solarus_vita_gc_hook)() = nullptr;\n\n" + a, 1)
    old = """      SDL_TEXTUREACCESS_TARGET,
      width,
      height);
"""
    assert old in s
    new = old + """#ifdef __vita__
  if (tex == nullptr && solarus_vita_gc_hook != nullptr) {
    solarus_vita_gc_hook();
    solarus_vita_gc_hook();
    tex = SDL_CreateTexture(
        renderer,
        Video::get_pixel_format()->format,
        SDL_TEXTUREACCESS_TARGET,
        width,
        height);
  }
#endif
"""
    return s.replace(old, new, 1)

def ml(s):
    if "vita_gc_now" in s:
        print("MainLoop deja patche"); return s
    anchor = "lua_context = std::unique_ptr<LuaContext>(new LuaContext(*this));\n"
    assert anchor in s
    s = s.replace(anchor, anchor + """#ifdef __vita__
  vita_gc_ctx = lua_context.get();
  solarus_vita_gc_hook = &vita_gc_now;
#endif
""", 1)
    inc = '#include "solarus/lua/LuaContext.h"\n'
    assert inc in s
    s = s.replace(inc, inc + """#ifdef __vita__
#include <lua.hpp>
#endif
""", 1)
    ns = "namespace Solarus {\n"
    assert ns in s
    s = s.replace(ns, ns + """
#ifdef __vita__
extern void (*solarus_vita_gc_hook)();
static LuaContext* vita_gc_ctx = nullptr;
static void vita_gc_now() {
  if (vita_gc_ctx != nullptr) {
    lua_gc(vita_gc_ctx->get_internal_state(), LUA_GCCOLLECT, 0);
  }
}
#endif
""", 1)
    return s

rw(root + "/src/graphics/sdlrenderer/SDLSurfaceImpl.cpp", sdl)
rw(root + "/src/core/MainLoop.cpp", ml)
print("PATCH_OK")
