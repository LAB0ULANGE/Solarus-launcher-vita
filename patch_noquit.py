def rw(path, fn):
    s = open(path, newline='').read()
    crlf = '\r\n' in s
    if crlf:
        s = s.replace('\r\n', '\n')
    s = fn(s)
    if crlf:
        s = s.replace('\n', '\r\n')
    open(path, 'w', newline='').write(s)

def fix_main(s):
    a = ("  if (event.is_window_closing()) {\n"
         "    set_exiting();\n"
         "  }\n")
    assert s.count(a) == 1, "MainLoop: ancre"
    new = ("  if (event.is_window_closing()) {\n"
           "#ifdef __vita__\n"
           "    // On the Vita the system suspends or kills the app itself: never\n"
           "    // quit on a close request, so that pressing PS keeps the game.\n"
           "    FILE* quit_log = fopen(\"ux0:data/solarus_debug.log\", \"a\");\n"
           "    if (quit_log) {\n"
           "      fprintf(quit_log, \"SOLARUS_DEBUG: window close request ignored\\n\");\n"
           "      fclose(quit_log);\n"
           "    }\n"
           "#else\n"
           "    set_exiting();\n"
           "#endif\n"
           "  }\n")
    return s.replace(a, new)

rw('src/core/MainLoop.cpp', fix_main)
print("PATCH OK")
