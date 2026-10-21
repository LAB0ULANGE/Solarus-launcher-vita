p = "src/graphics/sdlrenderer/SDLSurfaceImpl.cpp"
b = open(p, "rb").read()
crlf = b"\r\n" in b
s = b.decode("utf-8").replace("\r\n", "\n")
if "mallinfo" in s:
    print("deja patche")
else:
    old = '      fprintf(texlog, "TEX target %dx%d %s | libre: user=%dK cdram=%dK phycont=%dK\\n", width, height, tex ? "ok" : "FAILED", mi.size_user / 1024, mi.size_cdram / 1024, mi.size_phycont / 1024);\n'
    assert old in s
    new = '''      struct mallinfo mm = mallinfo();
      fprintf(texlog, "TEX target %dx%d %s | libre: user=%dK cdram=%dK phycont=%dK | tas: utilise=%dK arena=%dK\\n", width, height, tex ? "ok" : "FAILED", mi.size_user / 1024, mi.size_cdram / 1024, mi.size_phycont / 1024, (int)(mm.uordblks / 1024), (int)(mm.arena / 1024));
'''
    s = s.replace(old, new, 1)
    inc = "#include <psp2/kernel/sysmem.h>\n"
    assert inc in s
    s = s.replace(inc, inc + "#include <malloc.h>\n", 1)
    if crlf: s = s.replace("\n", "\r\n")
    open(p, "wb").write(s.encode("utf-8"))
    print("PATCH_OK")
