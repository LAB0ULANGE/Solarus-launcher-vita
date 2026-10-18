def rw(path, fn):
    s = open(path, newline='').read()
    crlf = '\r\n' in s
    if crlf:
        s = s.replace('\r\n', '\n')
    s = fn(s)
    if crlf:
        s = s.replace('\n', '\r\n')
    open(path, 'w', newline='').write(s)

def once(s, a, b, name):
    assert s.count(a) == 1, "ancre introuvable: " + name
    return s.replace(a, b)

def fix(s):
    assert "TEX target" not in s, "SDLSurfaceImpl.cpp deja patche"
    s = once(s, "#include <SDL_render.h>\n", "#include <SDL_render.h>\n#include <cstdio>\n", "include")
    a = ("  SDL_Texture* tex = SDL_CreateTexture(\n"
         "      renderer,\n"
         "      Video::get_pixel_format()->format,\n"
         "      SDL_TEXTUREACCESS_TARGET,\n"
         "      width,\n"
         "      height);\n")
    s = once(s, a, a +
        "#ifdef __vita__\n"
        "  {\n"
        "    FILE* texlog = fopen(\"ux0:data/solarus_debug.log\", \"a\");\n"
        "    if (texlog) {\n"
        "      fprintf(texlog, \"TEX target %dx%d %s\\n\", width, height, tex ? \"ok\" : \"FAILED\");\n"
        "      fclose(texlog);\n"
        "    }\n"
        "  }\n"
        "#endif\n", "create")
    a = "      std::string(\"Failed to create render texture : \") + SDL_GetError());"
    s = once(s, a, "      std::string(\"Failed to create render texture \") + std::to_string(width) + \"x\" +\n"
        "      std::to_string(height) + \" : \" + SDL_GetError());", "msg")
    return s

rw('src/graphics/sdlrenderer/SDLSurfaceImpl.cpp', fix)
print("TEXLOG_OK")
