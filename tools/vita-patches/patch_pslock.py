def rw(path, fn):
    s = open(path, newline='').read()
    crlf = '\r\n' in s
    if crlf:
        s = s.replace('\r\n', '\n')
    s = fn(s)
    if crlf:
        s = s.replace('\n', '\r\n')
    open(path, 'w', newline='').write(s)

def need(s, a, name):
    assert s.count(a) == 1, "MainLoop: ancre " + name

def fix_main(s):
    assert "vita_lock_ps" not in s, "deja patche"
    a = "#include <thread>\n\nnamespace Solarus {\n"
    need(s, a, "1")
    helper = ("#include <thread>\n\n"
              "#ifdef __vita__\n"
              "#include <psp2/shellutil.h>\n"
              "// While the engine loads, the PS button is locked: the Vita shows the\n"
              "// \"forbidden\" symbol instead of leaving the app in the middle of a load.\n"
              "static void vita_lock_ps(bool lock) {\n"
              "  static bool events_ready = false;\n"
              "  if (!events_ready) {\n"
              "    sceShellUtilInitEvents(0);\n"
              "    events_ready = true;\n"
              "  }\n"
              "  if (lock) {\n"
              "    sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);\n"
              "  }\n"
              "  else {\n"
              "    sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);\n"
              "  }\n"
              "}\n"
              "#endif\n\n"
              "namespace Solarus {\n")
    s = s.replace(a, helper)
    a = "  commands_dispatcher(*this)\n{\n"
    need(s, a, "2")
    s = s.replace(a, a + "#ifdef __vita__\n  vita_lock_ps(true);\n#endif\n")
    a = "void MainLoop::run() {\n  SOL_MAIN_THREAD;\n"
    need(s, a, "3")
    s = s.replace(a, a + "#ifdef __vita__\n  vita_lock_ps(false);\n#endif\n")
    a = "    game = std::unique_ptr<Game>(next_game);\n\n    if (game != nullptr) {\n      game->start();\n    }\n"
    need(s, a, "4")
    s = s.replace(a,
        "#ifdef __vita__\n    if (next_game != nullptr) {\n      vita_lock_ps(true);\n    }\n#endif\n"
        "    game = std::unique_ptr<Game>(next_game);\n\n    if (game != nullptr) {\n      game->start();\n"
        "#ifdef __vita__\n      vita_lock_ps(false);\n#endif\n    }\n")
    return s

def fix_cmake(s):
    if "SceShellUtil_stub" in s:
        return s
    assert s.count("SceNetCtl_stub") >= 1, "cmake: SceNetCtl_stub introuvable"
    return s.replace("SceNetCtl_stub", "SceNetCtl_stub SceShellUtil_stub", 1)

rw('src/core/MainLoop.cpp', fix_main)
rw('cmake/AddSolarusLibrary.cmake', fix_cmake)
print("PATCH OK")
