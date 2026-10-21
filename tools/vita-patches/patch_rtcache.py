import os
def rw(p, f):
    b = open(p, "rb").read()
    crlf = b"\r\n" in b
    s = b.decode("utf-8").replace("\r\n", "\n")
    s2 = f(s)
    if s2 is None: return
    if crlf: s2 = s2.replace("\n", "\r\n")
    open(p, "wb").write(s2.encode("utf-8"))

WRAP = r'''
#ifdef __vita__
// VITA_RT_CACHE: the Vita can only have a limited number of GPU render
// targets (SCE_GXM_ERROR_OUT_OF_RENDER_TARGETS). SDL creates one per target
// texture; we share them between textures of the same size.
#include <psp2/gxm.h>
#include <vector>
#include <cstdio>
extern "C" {
int __real_sceGxmCreateRenderTarget(const SceGxmRenderTargetParams* params, SceGxmRenderTarget** render_target);
int __real_sceGxmDestroyRenderTarget(SceGxmRenderTarget* render_target);
}
namespace {
struct VitaRtEntry {
  unsigned w, h, flags, msaa, spf;
  SceGxmRenderTarget* rt;
  int refs;
};
std::vector<VitaRtEntry>& vita_rt_cache() {
  static std::vector<VitaRtEntry> cache;
  return cache;
}
}
extern "C" int __wrap_sceGxmCreateRenderTarget(const SceGxmRenderTargetParams* p, SceGxmRenderTarget** out) {
  if (p == nullptr || out == nullptr || p->driverMemBlock != -1) {
    return __real_sceGxmCreateRenderTarget(p, out);
  }
  std::vector<VitaRtEntry>& cache = vita_rt_cache();
  for (VitaRtEntry& e : cache) {
    if (e.w == p->width && e.h == p->height && e.flags == p->flags &&
        e.msaa == p->multisampleMode && e.spf == p->scenesPerFrame) {
      ++e.refs;
      *out = e.rt;
      return 0;
    }
  }
  int err = __real_sceGxmCreateRenderTarget(p, out);
  if (err < 0) {
    // Free the cached render targets nobody uses anymore, then retry.
    for (auto it = cache.begin(); it != cache.end();) {
      if (it->refs == 0) {
        __real_sceGxmDestroyRenderTarget(it->rt);
        it = cache.erase(it);
      }
      else {
        ++it;
      }
    }
    err = __real_sceGxmCreateRenderTarget(p, out);
  }
  if (err >= 0) {
    cache.push_back({ p->width, p->height, p->flags, p->multisampleMode, p->scenesPerFrame, *out, 1 });
  }
  FILE* f = fopen("ux0:data/solarus_debug.log", "a");
  if (f) {
    fprintf(f, "RT new %ux%u %s (tailles en cache: %u)\n", (unsigned) p->width, (unsigned) p->height,
        err >= 0 ? "ok" : "FAILED", (unsigned) cache.size());
    fclose(f);
  }
  return err;
}
extern "C" int __wrap_sceGxmDestroyRenderTarget(SceGxmRenderTarget* rt) {
  for (VitaRtEntry& e : vita_rt_cache()) {
    if (e.rt == rt) {
      if (e.refs > 0) {
        --e.refs;
      }
      return 0;
    }
  }
  return __real_sceGxmDestroyRenderTarget(rt);
}
#endif
'''

def main_cpp(s):
    if "VITA_RT_CACHE" in s:
        print("main.cpp deja patche"); return None
    return s.rstrip("\n") + "\n" + WRAP

CM = '''
# VITA_RT_CACHE: share GPU render targets (see cli/src/main.cpp)
if(VITA OR DEFINED ENV{VITASDK})
  target_link_options(solarus-run PRIVATE "-Wl,--wrap=sceGxmCreateRenderTarget" "-Wl,--wrap=sceGxmDestroyRenderTarget")
endif()
'''

def cmake(s):
    if "VITA_RT_CACHE" in s:
        print("CMakeLists deja patche"); return None
    return s.rstrip("\n") + "\n" + CM

cm_file = None
for cand in ["cli/CMakeLists.txt", "CMakeLists.txt"]:
    if os.path.exists(cand):
        t = open(cand, encoding="utf-8", errors="replace").read()
        if "add_executable(solarus-run" in t.replace(" (", "(") or ("solarus-run" in t and "add_executable" in t):
            cm_file = cand
            break
assert cm_file, "fichier CMake de solarus-run introuvable"
print("CMake:", cm_file)
rw("cli/src/main.cpp", main_cpp)
rw(cm_file, cmake)
print("PATCH_OK")
