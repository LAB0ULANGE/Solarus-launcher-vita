p = "src/graphics/TextSurface.cpp"
b = open(p, "rb").read()
crlf = b"\r\n" in b
s = b.decode("utf-8").replace("\r\n", "\n")
if "VITA_TEXT_CPU" in s:
    print("deja patche")
else:
    a = "  surface = Surface::create((char_width - 1) * num_chars + 1, char_height);\n"
    assert s.count(a) == 1
    block = """#ifdef __vita__
  // VITA_TEXT_CPU: build the text in CPU memory and make a simple texture,
  // instead of a GPU render target (the Vita limits how many can exist).
  {
    SDL_Surface* src = bitmap->get_impl().get_surface();
    if (src != nullptr) {
      SDL_PixelFormat* format = Video::get_pixel_format();
      SDL_Surface_UniquePtr dst(SDL_CreateRGBSurface(
          0, (char_width - 1) * num_chars + 1, char_height, 32,
          format->Rmask, format->Gmask, format->Bmask, format->Amask));
      SOLARUS_REQUIRE(dst != nullptr,
          std::string("Failed to create text surface: ") + SDL_GetError());
      SDL_FillRect(dst.get(), nullptr, 0);
      SDL_BlendMode old_mode = SDL_BLENDMODE_NONE;
      SDL_GetSurfaceBlendMode(src, &old_mode);
      SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_BLEND);
      int dst_x = 0;
      for (unsigned i = 0; i < text.size(); i++) {
        char first_byte = text[i];
        SDL_Rect src_rect = { 0, 0, char_width, char_height };
        if ((first_byte & 0xE0) != 0xC0) {
          src_rect.x = first_byte * char_width;
        }
        else {
          ++i;
          char second_byte = text[i];
          uint16_t code_point = ((first_byte & 0x1F) << 6) | (second_byte & 0x3F);
          src_rect.x = (code_point % 128) * char_width;
          src_rect.y = (code_point / 128) * char_height;
        }
        SDL_Rect dst_rect = { dst_x, 0, char_width, char_height };
        SDL_BlitSurface(src, &src_rect, dst.get(), &dst_rect);
        dst_x += char_width - 1;
      }
      SDL_SetSurfaceBlendMode(src, old_mode);
      surface = Surface::create(std::move(dst), true);
      return;
    }
  }
#endif
"""
    s = s.replace(a, block + a, 1)
    if crlf: s = s.replace("\n", "\r\n")
    open(p, "wb").write(s.encode("utf-8"))
    print("PATCH_OK")
