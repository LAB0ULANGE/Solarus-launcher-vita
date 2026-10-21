import re

def rw(path, fn):
    s = open(path, newline='').read()
    crlf = '\r\n' in s
    if crlf:
        s = s.replace('\r\n', '\n')
    s = fn(s)
    if crlf:
        s = s.replace('\n', '\r\n')
    open(path, 'w', newline='').write(s)

# ---------------------------------------------------------------- MainApi.cpp
VITA_API = r'''#ifdef __vita__
namespace {

// ----- Vita launcher support: download (non-blocking), launch, file access.

char vita_net_mem[1024 * 1024];
bool vita_net_ready = false;

CURLM* dl_multi = nullptr;
CURL* dl_easy = nullptr;
FILE* dl_file = nullptr;
std::string dl_state = "idle";
std::string dl_error;
std::string dl_dest;
std::string dl_part;
curl_off_t dl_bytes = 0;
curl_off_t dl_total = 0;
char dl_errbuf[CURL_ERROR_SIZE];

size_t dl_write(void* p, size_t size, size_t n, void* user) {
  return fwrite(p, size, n, static_cast<FILE*>(user));
}

void vita_net_init() {
  if (vita_net_ready) {
    return;
  }
  sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
  SceNetInitParam init_param;
  init_param.memory = vita_net_mem;
  init_param.size = sizeof(vita_net_mem);
  init_param.flags = 0;
  sceNetInit(&init_param);
  sceNetCtlInit();
  curl_global_init(CURL_GLOBAL_DEFAULT);
  vita_net_ready = true;
}

void dl_cleanup() {
  if (dl_easy != nullptr && dl_multi != nullptr) {
    curl_multi_remove_handle(dl_multi, dl_easy);
  }
  if (dl_easy != nullptr) {
    curl_easy_cleanup(dl_easy);
    dl_easy = nullptr;
  }
  if (dl_multi != nullptr) {
    curl_multi_cleanup(dl_multi);
    dl_multi = nullptr;
  }
  if (dl_file != nullptr) {
    fclose(dl_file);
    dl_file = nullptr;
  }
}

void vita_write_text(const char* path, const std::string& text) {
  FILE* f = fopen(path, "w");
  if (f != nullptr) {
    fputs(text.c_str(), f);
    fclose(f);
  }
}

/** sol.main.vita_download_start(url, dest_path) -> boolean */
int vita_download_start(lua_State* l) {
  const char* url = luaL_checkstring(l, 1);
  const char* dest = luaL_checkstring(l, 2);
  dl_cleanup();
  vita_net_init();
  dl_dest = dest;
  dl_part = dl_dest + ".part";
  dl_bytes = 0;
  dl_total = 0;
  dl_error.clear();
  dl_errbuf[0] = '\0';
  dl_file = fopen(dl_part.c_str(), "wb");
  if (dl_file == nullptr) {
    dl_state = "error";
    dl_error = "cannot write " + dl_part;
    lua_pushboolean(l, 0);
    return 1;
  }
  dl_multi = curl_multi_init();
  dl_easy = curl_easy_init();
  curl_easy_setopt(dl_easy, CURLOPT_URL, url);
  curl_easy_setopt(dl_easy, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(dl_easy, CURLOPT_MAXREDIRS, 10L);
  curl_easy_setopt(dl_easy, CURLOPT_WRITEFUNCTION, dl_write);
  curl_easy_setopt(dl_easy, CURLOPT_WRITEDATA, dl_file);
  curl_easy_setopt(dl_easy, CURLOPT_USERAGENT, "SolarusVita/0.1");
  curl_easy_setopt(dl_easy, CURLOPT_CONNECTTIMEOUT, 20L);
  curl_easy_setopt(dl_easy, CURLOPT_ERRORBUFFER, dl_errbuf);
  // TODO: ship a CA bundle and enable certificate verification.
  curl_easy_setopt(dl_easy, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(dl_easy, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_multi_add_handle(dl_multi, dl_easy);
  dl_state = "running";
  lua_pushboolean(l, 1);
  return 1;
}

/** sol.main.vita_download_poll() -> { state, bytes, total, error } */
int vita_download_poll(lua_State* l) {
  if (dl_state == "running" && dl_multi != nullptr) {
    int running = 0;
    curl_multi_perform(dl_multi, &running);
    curl_easy_getinfo(dl_easy, CURLINFO_SIZE_DOWNLOAD_T, &dl_bytes);
    curl_easy_getinfo(dl_easy, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &dl_total);
    if (running == 0) {
      bool ok = false;
      int msgs = 0;
      CURLMsg* msg;
      while ((msg = curl_multi_info_read(dl_multi, &msgs)) != nullptr) {
        if (msg->msg == CURLMSG_DONE) {
          long code = 0;
          curl_easy_getinfo(dl_easy, CURLINFO_RESPONSE_CODE, &code);
          if (msg->data.result == CURLE_OK && code == 200) {
            ok = true;
          }
          else if (msg->data.result != CURLE_OK) {
            dl_error = curl_easy_strerror(msg->data.result);
          }
          else {
            dl_error = "HTTP " + std::to_string(code);
          }
        }
      }
      dl_cleanup();
      if (ok) {
        sceIoRemove(dl_dest.c_str());
        if (sceIoRename(dl_part.c_str(), dl_dest.c_str()) >= 0) {
          dl_state = "done";
        }
        else {
          dl_state = "error";
          dl_error = "cannot rename downloaded file";
        }
      }
      else {
        sceIoRemove(dl_part.c_str());
        dl_state = "error";
        if (dl_error.empty()) {
          dl_error = "download failed";
        }
      }
    }
  }
  lua_newtable(l);
  lua_pushstring(l, dl_state.c_str());
  lua_setfield(l, -2, "state");
  lua_pushnumber(l, static_cast<lua_Number>(dl_bytes));
  lua_setfield(l, -2, "bytes");
  lua_pushnumber(l, static_cast<lua_Number>(dl_total));
  lua_setfield(l, -2, "total");
  lua_pushstring(l, dl_error.c_str());
  lua_setfield(l, -2, "error");
  return 1;
}

/** sol.main.vita_download_cancel() */
int vita_download_cancel(lua_State* /* l */) {
  dl_cleanup();
  if (!dl_part.empty()) {
    sceIoRemove(dl_part.c_str());
  }
  dl_state = "idle";
  return 0;
}

/** sol.main.vita_file_size(path) -> number or nil */
int vita_file_size(lua_State* l) {
  const char* path = luaL_checkstring(l, 1);
  FILE* f = fopen(path, "rb");
  if (f == nullptr) {
    lua_pushnil(l);
    return 1;
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  lua_pushnumber(l, static_cast<lua_Number>(size));
  return 1;
}

/** sol.main.vita_read_file(path) -> string, or nil + message */
int vita_read_file(lua_State* l) {
  const char* path = luaL_checkstring(l, 1);
  FILE* f = fopen(path, "rb");
  if (f == nullptr) {
    lua_pushnil(l);
    lua_pushstring(l, "cannot open file");
    return 2;
  }
  std::string content;
  char buffer[4096];
  size_t n;
  while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
    content.append(buffer, n);
  }
  fclose(f);
  lua_pushlstring(l, content.data(), content.size());
  return 1;
}

/** sol.main.vita_launch(quest_path) -> boolean (only returns on failure) */
int vita_launch(lua_State* l) {
  const char* quest = luaL_checkstring(l, 1);
  vita_write_text("ux0:data/Solarus/quest.txt", std::string(quest) + "\n");
  vita_write_text("ux0:data/Solarus/mode.txt", "play\n");
  int result = sceAppMgrLoadExec("app0:eboot.bin", nullptr, nullptr);
  lua_pushboolean(l, result >= 0);
  return 1;
}

/** sol.main.vita_back_to_launcher() -> boolean (only returns on failure) */
int vita_back_to_launcher(lua_State* l) {
  sceIoRemove("ux0:data/Solarus/mode.txt");
  int result = sceAppMgrLoadExec("app0:eboot.bin", nullptr, nullptr);
  lua_pushboolean(l, result >= 0);
  return 1;
}

}  // Anonymous namespace.
#endif

'''

VITA_INCLUDES = '''#ifdef __vita__
#include <curl/curl.h>
#include <psp2/appmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <cstdio>
#include <string>
#endif
'''

VITA_REG = '''
#ifdef __vita__
      ,{ "vita_download_start", vita_download_start }
      ,{ "vita_download_poll", vita_download_poll }
      ,{ "vita_download_cancel", vita_download_cancel }
      ,{ "vita_file_size", vita_file_size }
      ,{ "vita_read_file", vita_read_file }
      ,{ "vita_launch", vita_launch }
      ,{ "vita_back_to_launcher", vita_back_to_launcher }
#endif
'''

def mainapi(s):
    a = '/**\n * \\brief Initializes the main features provided to Lua.'
    assert s.count(a) == 1, "MainApi: anchor 1"
    s = s.replace(a, VITA_API + a)
    b = '{ "get_os", LUA_TO_C_BIND(System::get_os) }'
    assert s.count(b) == 1, "MainApi: anchor 2"
    s = s.replace(b, b + VITA_REG.rstrip('\n'))
    assert s.count('#include') >= 1, "MainApi: no include"
    s = s.replace('#include', VITA_INCLUDES + '#include', 1)
    return s

# --------------------------------------------------------------- MainLoop.cpp
NEW_QUEST_BLOCK = r'''#ifdef __vita__
  {
    std::string vita_quest;
    // "play" mode: written by the launcher just before it restarts the app.
    bool play_mode = false;
    FILE* mf = fopen("ux0:data/Solarus/mode.txt", "r");
    if (mf) {
      char mode[16] = { 0 };
      if (fgets(mode, sizeof(mode), mf) &&
          mode[0] == 'p' && mode[1] == 'l' && mode[2] == 'a' && mode[3] == 'y') {
        play_mode = true;
      }
      fclose(mf);
      remove("ux0:data/Solarus/mode.txt");
    }
    // A quest bundled in the VPK (app0:data/) is the launcher: it runs
    // unless the launcher asked to play a downloaded quest.
    FILE* bundled = fopen("app0:data/quest.dat", "rb");
    if (bundled) {
      fclose(bundled);
    }
    FILE* qf = (bundled && !play_mode) ? nullptr : fopen("ux0:data/Solarus/quest.txt", "r");
    if (qf) {
      char line[512];
      if (fgets(line, sizeof(line), qf)) {
        vita_quest = line;
        while (!vita_quest.empty() &&
            (vita_quest.back() == '\n' || vita_quest.back() == '\r' || vita_quest.back() == ' ')) {
          vita_quest.pop_back();
        }
      }
      fclose(qf);
    }
    FILE* dbgf = fopen("ux0:data/solarus_debug.log", "a");
    if (dbgf) {
      fprintf(dbgf, "SOLARUS_DEBUG: play_mode=%d quest.txt path='%s'\n", play_mode ? 1 : 0, vita_quest.c_str());
      fclose(dbgf);
    }
    if (!vita_quest.empty()) {
      return vita_quest;
    }
  }
#endif
'''

def mainloop(s):
    pat = re.compile(r'#ifdef __vita__\n  \{\n    std::string vita_quest;.*?\n#endif\n', re.S)
    assert len(pat.findall(s)) == 1, "MainLoop: quest block not found"
    return pat.sub(lambda m: NEW_QUEST_BLOCK, s)

# ------------------------------------------------------ AddSolarusLibrary.cmake
def cmake(s):
    a = '    webp\n    SceAppMgr_stub'
    assert s.count(a) == 1, "cmake: anchor"
    return s.replace(a, '    webp\n    curl\n    ssl\n    crypto\n    zstd\n    SceNet_stub\n    SceNetCtl_stub\n    SceAppMgr_stub')

rw('src/lua/MainApi.cpp', mainapi)
rw('src/core/MainLoop.cpp', mainloop)
rw('cmake/AddSolarusLibrary.cmake', cmake)
print("PATCH OK")
