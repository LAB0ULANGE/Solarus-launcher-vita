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
    assert "purge unused" not in s, "Surface.cpp deja patche"
    a = "    texture = Video::get_renderer().create_texture(create_sdl_surface_from_file(actual_file_name));\n"
    assert s.count(a) == 1, "ancre introuvable"
    b = ("#ifdef __vita__\n"
         "    // Vita: purge unused images (only the cache still holds them) to\n"
         "    // free video memory before loading a new one.\n"
         "    for (auto purge_it = image_files_cache.begin(); purge_it != image_files_cache.end();) {\n"
         "      if (purge_it->second.use_count() == 1) {\n"
         "        purge_it = image_files_cache.erase(purge_it);\n"
         "      }\n"
         "      else {\n"
         "        ++purge_it;\n"
         "      }\n"
         "    }\n"
         "#endif\n")
    return s.replace(a, b + a)

rw('src/graphics/Surface.cpp', fix)
print("PURGE_OK")
