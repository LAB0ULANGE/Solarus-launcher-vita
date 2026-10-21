p = "src/core/QuestFiles.cpp"
b = open(p, "rb").read()
crlf = b"\r\n" in b
s = b.decode("utf-8").replace("\r\n", "\n")
if "VITA_OVERRIDES_BY_ID" in s:
    print("deja patche")
    raise SystemExit
old = """  set_quest_write_dir(CurrentQuest::get_properties().get_quest_write_dir());

  return true;
"""
new = """  set_quest_write_dir(CurrentQuest::get_properties().get_quest_write_dir());

#ifdef __vita__
  {
    // VITA_OVERRIDES_BY_ID: replacement files chosen by the quest's own id
    // (write_dir in quest.dat), so a renamed .solarus file still matches.
    // Priority: ux0 (tests) > app0 (shipped in the VPK) > the quest itself.
    const std::string quest_id = CurrentQuest::get_properties().get_quest_write_dir();
    if (!quest_id.empty()) {
      const std::string app_dir = "app0:overrides/" + quest_id;
      const std::string ux_dir = "ux0:data/Solarus/overrides/" + quest_id;
      const int app_ok = PHYSFS_mount(app_dir.c_str(), nullptr, 0);
      const int ux_ok = PHYSFS_mount(ux_dir.c_str(), nullptr, 0);
      FILE* dbg = fopen("ux0:data/solarus_debug.log", "a");
      if (dbg) {
        fprintf(dbg, "OVERRIDES id=%s app0=%s ux0=%s\\n", quest_id.c_str(),
            app_ok ? "oui" : "non", ux_ok ? "oui" : "non");
        fclose(dbg);
      }
    }
  }
#endif

  return true;
"""
assert s.count(old) == 1, "motif introuvable"
s = s.replace(old, new, 1)
if crlf: s = s.replace("\n", "\r\n")
open(p, "wb").write(s.encode("utf-8"))
print("PATCH_OK")
