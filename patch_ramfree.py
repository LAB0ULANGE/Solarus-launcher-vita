# Libere la copie en memoire des tilesets (gros PNG) une fois envoyes a l'ecran.
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

def fix_h(s):
    assert "release_cpu_copy" not in s, "SDLSurfaceImpl.h deja patche"
    s = once(s, "#include <memory>\n", "#include <memory>\n#include <string>\n", "h include")
    s = once(s, "  void upload_surface() override;\nprivate:\n",
        "  void upload_surface() override;\n\n"
        "  // Frees the CPU copy of an image that is only drawn (it is reloaded\n"
        "  // from the quest file if some code asks for the pixels later).\n"
        "  void release_cpu_copy(const std::string& file_name);\n"
        "private:\n  std::string source_file;\n", "h members")
    return s

def fix_cpp(s):
    assert "release_cpu_copy" not in s, "SDLSurfaceImpl.cpp deja patche"
    s = once(s, "#include <SDL_render.h>\n",
        "#include \"solarus/core/QuestFiles.h\"\n#include <SDL_render.h>\n#include <SDL_image.h>\n", "cpp include")
    a = "SDL_Surface* SDLSurfaceImpl::get_surface() const {\n"
    s = once(s, a, a +
        "  if (!surface && !target && !source_file.empty()) {\n"
        "    // The CPU copy was released: reload it from the quest file.\n"
        "    const std::string& buffer = QuestFiles::data_file_read(source_file);\n"
        "    SDL_RWops* rw = SDL_RWFromMem(const_cast<char*>(buffer.data()), (int) buffer.size());\n"
        "    SDL_Surface_UniquePtr loaded = SDL_Surface_UniquePtr(IMG_Load_RW(rw, 0));\n"
        "    SDL_RWclose(rw);\n"
        "    SOLARUS_REQUIRE(loaded != nullptr,\n"
        "        std::string(\"Cannot reload image '\") + source_file + \"': \" + SDL_GetError());\n"
        "    SDL_PixelFormat* pixel_format = Video::get_pixel_format();\n"
        "    if (loaded->format->format != pixel_format->format) {\n"
        "      SDL_Surface_UniquePtr converted = SDL_Surface_UniquePtr(\n"
        "          SDL_ConvertSurface(loaded.get(), pixel_format, 0));\n"
        "      SOLARUS_REQUIRE(converted != nullptr,\n"
        "          std::string(\"Failed to convert software surface: \") + SDL_GetError());\n"
        "      loaded = std::move(converted);\n"
        "    }\n"
        "    surface = std::move(loaded);\n"
        "  }\n", "get_surface")
    a = "/**\n * \\copydoc SurfaceImpl::get_texture\n */"
    s = once(s, a,
        "void SDLSurfaceImpl::release_cpu_copy(const std::string& file_name) {\n"
        "  if (!target && surface != nullptr) {\n"
        "    source_file = file_name;\n"
        "    surface.reset();\n"
        "  }\n"
        "}\n\n" + a, "release")
    return s

def fix_surface(s):
    assert "release_cpu_copy" not in s, "Surface.cpp deja patche"
    s = once(s, "#include <SDL_render.h>\n#include <SDL_image.h>\n",
        "#include <SDL_render.h>\n#include <SDL_image.h>\n#ifdef __vita__\n"
        "#include \"solarus/graphics/sdlrenderer/SDLSurfaceImpl.h\"\n#endif\n", "include")
    a = ("    texture = Video::get_renderer().create_texture(create_sdl_surface_from_file(actual_file_name));\n")
    s = once(s, a, a +
        "#ifdef __vita__\n"
        "    // Tilesets are only drawn: free their CPU copy to save memory.\n"
        "    if (actual_file_name.size() > 10 &&\n"
        "        actual_file_name.compare(actual_file_name.size() - 10, 10, \".tiles.png\") == 0) {\n"
        "      SDLSurfaceImpl* sdl_impl = dynamic_cast<SDLSurfaceImpl*>(texture.get());\n"
        "      if (sdl_impl != nullptr) {\n"
        "        sdl_impl->release_cpu_copy(actual_file_name);\n"
        "      }\n"
        "    }\n"
        "#endif\n", "create_texture")
    return s

rw('include/solarus/graphics/sdlrenderer/SDLSurfaceImpl.h', fix_h)
rw('src/graphics/sdlrenderer/SDLSurfaceImpl.cpp', fix_cpp)
rw('src/graphics/Surface.cpp', fix_surface)
print("RAM_OK")
