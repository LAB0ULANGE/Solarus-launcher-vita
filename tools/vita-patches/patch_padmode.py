p = "src/core/MainLoop.cpp"
b = open(p, "rb").read()
crlf = b"\r\n" in b
s = b.decode("utf-8").replace("\r\n", "\n")
if "vita_pad_mode" in s:
    print("deja patche")
    raise SystemExit
R = []
R.append(("bool vita_map_exists = false;\n",
          "bool vita_map_exists = false;\nbool vita_pad_mode = true;  // true: the game receives the Vita buttons itself.\n"))
R.append(("  vita_defaults();\n  vita_map_exists = false;\n",
          "  vita_defaults();\n  vita_map_exists = false;\n  vita_pad_mode = !vita_is_launcher();\n"))
R.append(("    SDL_Keycode k = SDL_GetKeyFromName(value);\n",
          "    if (strcmp(line, \"mode\") == 0) {\n      vita_pad_mode = (strcmp(value, \"keys\") != 0);\n      continue;\n    }\n    SDL_Keycode k = SDL_GetKeyFromName(value);\n"))
R.append(("  for (int i = 0; i < vita_num_pad_buttons; ++i) {\n    const char* name = \"none\";\n",
          "  fprintf(f, \"mode=%s\\n\", vita_pad_mode ? \"pad\" : \"keys\");\n  for (int i = 0; i < vita_num_pad_buttons; ++i) {\n    const char* name = \"none\";\n"))
R.append(("""    if (b == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
      return false;  // Reserved: stretched or centered display.
    }
""", """    if (b == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
      return false;  // Reserved: stretched or centered display.
    }
    if (vita_pad_mode) {
      return false;  // Pad mode: the game receives the Vita buttons itself.
    }
"""))
R.append(("void vita_menu_refresh() {\n", """// Commands screen items: mode, [7 buttons in keyboard mode], defaults, back.
int vita_cmd_count() {
  return vita_pad_mode ? 3 : vita_num_pad_buttons + 3;
}

void vita_toggle_mode() {
  vita_release_all_keys();
  vita_pad_mode = !vita_pad_mode;
  vita_save_map();
  vita_menu_sel = 0;
}

void vita_menu_refresh() {
"""))
R.append(("""    const std::vector<VitaKeyChoice>& choices = vita_key_choices();
    for (int i = 0; i < vita_num_pad_buttons; ++i) {
      lines.push_back((vita_menu_sel == i ? cursor : none) +
          vita_pad_name(vita_pad_buttons[i].label, 10) + ": " +
          choices[vita_choice_index(vita_pad_key[i])].label);
    }
    lines.push_back((vita_menu_sel == vita_num_pad_buttons ? cursor : none) + "REMETTRE LES COMMANDES PAR DEFAUT");
    lines.push_back((vita_menu_sel == vita_num_pad_buttons + 1 ? cursor : none) + "RETOUR");
    lines.push_back("");
    lines.push_back("~GAUCHE DROITE CHANGER LA TOUCHE   ROND RETOUR");
""", """    const std::vector<VitaKeyChoice>& choices = vita_key_choices();
    lines.push_back((vita_menu_sel == 0 ? cursor : none) +
        std::string(vita_pad_mode ? "MODE : MANETTE" : "MODE : CLAVIER"));
    if (vita_pad_mode) {
      lines.push_back("~LE JEU RECOIT LES BOUTONS VITA");
      lines.push_back("~REGLAGE DANS LE MENU CONTROLES DU JEU");
    }
    else {
      for (int i = 0; i < vita_num_pad_buttons; ++i) {
        lines.push_back((vita_menu_sel == i + 1 ? cursor : none) +
            vita_pad_name(vita_pad_buttons[i].label, 10) + ": " +
            choices[vita_choice_index(vita_pad_key[i])].label);
      }
    }
    const int n = vita_cmd_count();
    lines.push_back((vita_menu_sel == n - 2 ? cursor : none) + "REMETTRE LES COMMANDES PAR DEFAUT");
    lines.push_back((vita_menu_sel == n - 1 ? cursor : none) + "RETOUR");
    lines.push_back("");
    lines.push_back("~GAUCHE DROITE CHANGER   ROND RETOUR");
"""))
R.append(("    case VITA_MENU_COMMANDS: return vita_num_pad_buttons + 2;\n",
          "    case VITA_MENU_COMMANDS: return vita_cmd_count();\n"))
R.append(("""    case VITA_MENU_COMMANDS:
      if (vita_menu_sel < vita_num_pad_buttons) {
        vita_menu_change_key(vita_menu_sel, 1);
      }
      else if (vita_menu_sel == vita_num_pad_buttons) {
        vita_defaults();
        vita_save_map();
      }
""", """    case VITA_MENU_COMMANDS:
      if (vita_menu_sel == 0) {
        vita_toggle_mode();
      }
      else if (vita_menu_sel < vita_cmd_count() - 2) {
        vita_menu_change_key(vita_menu_sel - 1, 1);
      }
      else if (vita_menu_sel == vita_cmd_count() - 2) {
        vita_defaults();
        vita_save_map();
      }
"""))
R.append(("""    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
      if (vita_menu_mode == VITA_MENU_COMMANDS && vita_menu_sel < vita_num_pad_buttons) {
        vita_menu_change_key(vita_menu_sel, -1);
      }
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      if (vita_menu_mode == VITA_MENU_COMMANDS && vita_menu_sel < vita_num_pad_buttons) {
        vita_menu_change_key(vita_menu_sel, 1);
      }
      break;
""", """    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      if (vita_menu_mode == VITA_MENU_COMMANDS) {
        if (vita_menu_sel == 0) {
          vita_toggle_mode();
        }
        else if (vita_menu_sel < vita_cmd_count() - 2) {
          vita_menu_change_key(vita_menu_sel - 1,
              button == SDL_CONTROLLER_BUTTON_DPAD_LEFT ? -1 : 1);
        }
      }
      break;
"""))
R.append(("""  if (!pressed) {
    switch (button) {
      case SDL_CONTROLLER_BUTTON_DPAD_UP: vita_push_key(SDLK_UP, false); break;
""", """  if (!pressed) {
    if (vita_pad_mode && released &&
        button != SDL_CONTROLLER_BUTTON_DPAD_UP && button != SDL_CONTROLLER_BUTTON_DPAD_DOWN &&
        button != SDL_CONTROLLER_BUTTON_DPAD_LEFT && button != SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
      return false;  // Let the game see the release of a button held before the menu.
    }
    switch (button) {
      case SDL_CONTROLLER_BUTTON_DPAD_UP: vita_push_key(SDLK_UP, false); break;
"""))
for old, new in R:
    assert s.count(old) == 1, "motif introuvable : " + old[:70]
    s = s.replace(old, new, 1)
if crlf: s = s.replace("\n", "\r\n")
open(p, "wb").write(s.encode("utf-8"))
print("PATCH_OK")
