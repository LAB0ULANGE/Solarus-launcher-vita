-- Solarus Vita launcher: lists the downloadable games published on
-- solarus-games.org, downloads them and starts them.

local INDEX_URL = "https://www.solarus-games.org/games/index.json"
local INDEX_PATH = "ux0:data/Solarus/index.json"
local QUESTS_DIR = "ux0:data/Solarus/quests/"


-- Games that do not work well on the Vita (or that already have a native
-- Vita port): a game is hidden if its file name contains one of these words.
local blocked_words = { "roth", "yarntown" }  -- yarntown: too slow on the Vita for now

-- Fallback list, used when the index cannot be downloaded or read.
local fallback_games = {
  {
    name = "Mystery of Solarus XD",
    url = "https://gitlab.com/solarus-games/games/zsxd/-/releases/v1.12.2/downloads/zsxd-v1.12.2.solarus",
    file = "zsxd-v1.12.2.solarus",
    size = 4575926,
    excerpt = "",
  },
}

-- Buttons: SDL controller numbers -> names (in case numbers are given).
local button_names = {
  [0] = "a", [1] = "b", [2] = "x", [3] = "y", [4] = "back", [6] = "start",
  [11] = "dpad_up", [12] = "dpad_down", [13] = "dpad_left", [14] = "dpad_right",
}

---------------------------------------------------------------------------
-- Texts in the language of the console (French, or English otherwise).
---------------------------------------------------------------------------
local lang = "fr"
if sol.main.vita_get_language ~= nil then
  lang = sol.main.vita_get_language()
end

local TEXTS = {
  fr = {
    installed = "Installé",
    on_pc = "Disponible sur PC",
    soon = "Pas encore sorti",
    to_download = "À télécharger",
    mb = "Mo",
    loading_game = "Chargement de %s...",
    cannot_launch = "Impossible de lancer le jeu.",
    cannot_download = "Impossible de lancer le téléchargement.",
    download_failed = "Échec du téléchargement : ",
    downloading = "Téléchargement...",
    loading_list = "Chargement de la liste des jeux...",
    offline_saved = "hors ligne : liste enregistrée",
    offline = "hors ligne",
    games_count = "%d jeux",
    added_by_hand = "Jeu ajouté à la main.",
    f_status = "État", f_developer = "Développeur", f_genre = "Genre", f_date = "Sortie",
    f_version = "Version", f_players = "Joueurs", f_languages = "Langues",
    players_range = "%d à %d",
    solarus_page = "Fiche Solarus :",
    solarus_page_url = "solarus-games.org/fr/games/",
    game_site = "Site du jeu :",
    f_platforms = "Aussi sur",
    uninstall_question = "Désinstaller ce jeu ?",
    yes_no = "Croix : oui       Rond : non",
    saves_question = "Supprimer aussi les sauvegardes ?",
    saves_keep_1 = "Si tu les gardes, tu retrouveras ta partie",
    saves_keep_2 = "en réinstallant le jeu.",
    delete_keep = "Croix : supprimer       Rond : garder",
    saves_deleted = "Les sauvegardes ont été supprimées.",
    saves_kept = "Les sauvegardes sont gardées.",
    cannot_delete = "Impossible de supprimer le jeu.",
    uninstalled = "Le jeu a été désinstallé.",
    press_button = "Appuie sur un bouton",
    press_to_close = "Appuie sur un bouton pour fermer",
    hint_installed = "Croix : jouer       Triangle : infos       Rond : désinstaller",
    hint_download = "Croix : télécharger       Triangle : infos",
    hint_info = "Triangle : infos",
    hint_cancel = "Rond : annuler",
    hint_details_play = "Croix : jouer     Rond : retour     Gauche / Droite : images     Haut / Bas : texte",
    hint_details_download = "Croix : télécharger     Rond : retour     Gauche / Droite : images     Haut / Bas : texte",
    hint_details = "Rond : retour     Gauche / Droite : images     Haut / Bas : texte",
    genres = {
      ["Adventure"] = "Aventure", ["Comedy"] = "Comédie", ["Puzzle"] = "Réflexion",
      ["Platformer"] = "Plates-formes", ["Shooter"] = "Tir", ["Horror"] = "Horreur",
    },
    languages = {
      en = "anglais", fr = "français", es = "espagnol", de = "allemand", it = "italien",
      pt = "portugais", ja = "japonais", zh = "chinois", ru = "russe", nl = "néerlandais",
      pl = "polonais", ko = "coréen", sv = "suédois", tr = "turc",
    },
  },
  en = {
    installed = "Installed",
    on_pc = "Available on PC",
    soon = "Not released yet",
    to_download = "Download",
    mb = "MB",
    loading_game = "Loading %s...",
    cannot_launch = "Cannot start the game.",
    cannot_download = "Cannot start the download.",
    download_failed = "Download failed: ",
    downloading = "Downloading...",
    loading_list = "Loading the game list...",
    offline_saved = "offline: saved list",
    offline = "offline",
    games_count = "%d games",
    added_by_hand = "Game added by hand.",
    f_status = "Status", f_developer = "Developer", f_genre = "Genre", f_date = "Release",
    f_version = "Version", f_players = "Players", f_languages = "Languages",
    players_range = "%d to %d",
    solarus_page = "Solarus page:",
    solarus_page_url = "solarus-games.org/games/",
    game_site = "Game website:",
    f_platforms = "Also on",
    uninstall_question = "Uninstall this game?",
    yes_no = "Cross: yes       Circle: no",
    saves_question = "Also delete the saved games?",
    saves_keep_1 = "If you keep them, you will find your progress",
    saves_keep_2 = "again when you reinstall the game.",
    delete_keep = "Cross: delete       Circle: keep",
    saves_deleted = "The saved games were deleted.",
    saves_kept = "The saved games were kept.",
    cannot_delete = "Cannot delete the game.",
    uninstalled = "The game was uninstalled.",
    press_button = "Press a button",
    press_to_close = "Press a button to close",
    hint_installed = "Cross: play       Triangle: info       Circle: uninstall",
    hint_download = "Cross: download       Triangle: info",
    hint_info = "Triangle: info",
    hint_cancel = "Circle: cancel",
    hint_details_play = "Cross: play     Circle: back     Left / Right: pictures     Up / Down: text",
    hint_details_download = "Cross: download     Circle: back     Left / Right: pictures     Up / Down: text",
    hint_details = "Circle: back     Left / Right: pictures     Up / Down: text",
    genres = {},
    languages = {
      en = "English", fr = "French", es = "Spanish", de = "German", it = "Italian",
      pt = "Portuguese", ja = "Japanese", zh = "Chinese", ru = "Russian", nl = "Dutch",
      pl = "Polish", ko = "Korean", sv = "Swedish", tr = "Turkish",
    },
  },
}
local T = TEXTS[lang] or TEXTS.en

---------------------------------------------------------------------------
-- Minimal JSON decoder (LuaJIT has no utf8 library, so we encode by hand).
---------------------------------------------------------------------------
local NULL = {}

local function utf8_char(cp)
  if cp < 0x80 then
    return string.char(cp)
  elseif cp < 0x800 then
    return string.char(0xC0 + math.floor(cp / 0x40), 0x80 + cp % 0x40)
  elseif cp < 0x10000 then
    return string.char(0xE0 + math.floor(cp / 0x1000),
        0x80 + math.floor(cp / 0x40) % 0x40, 0x80 + cp % 0x40)
  else
    return string.char(0xF0 + math.floor(cp / 0x40000),
        0x80 + math.floor(cp / 0x1000) % 0x40,
        0x80 + math.floor(cp / 0x40) % 0x40, 0x80 + cp % 0x40)
  end
end

local escapes = { b = "\b", f = "\f", n = "\n", r = "\r", t = "\t" }

local function decode(str)
  local pos = 1

  local function skip()
    pos = str:find("[^ \n\r\t]", pos) or (#str + 1)
  end

  local parse_value

  local function parse_string()
    pos = pos + 1  -- opening quote
    local buf = {}
    while true do
      local c = str:sub(pos, pos)
      if c == "" then
        error("unterminated string")
      elseif c == '"' then
        pos = pos + 1
        break
      elseif c == "\\" then
        local e = str:sub(pos + 1, pos + 1)
        if e == "u" then
          local cp = tonumber(str:sub(pos + 2, pos + 5), 16) or 63
          pos = pos + 6
          if cp >= 0xD800 and cp <= 0xDBFF and str:sub(pos, pos + 1) == "\\u" then
            local lo = tonumber(str:sub(pos + 2, pos + 5), 16)
            if lo and lo >= 0xDC00 and lo <= 0xDFFF then
              cp = 0x10000 + (cp - 0xD800) * 0x400 + (lo - 0xDC00)
              pos = pos + 6
            end
          end
          buf[#buf + 1] = utf8_char(cp)
        else
          buf[#buf + 1] = escapes[e] or e
          pos = pos + 2
        end
      else
        local s, f = str:find('^[^"\\]+', pos)
        buf[#buf + 1] = str:sub(s, f)
        pos = f + 1
      end
    end
    return table.concat(buf)
  end

  function parse_value()
    skip()
    local c = str:sub(pos, pos)
    if c == "{" then
      pos = pos + 1
      local obj = {}
      skip()
      if str:sub(pos, pos) == "}" then
        pos = pos + 1
        return obj
      end
      while true do
        skip()
        if str:sub(pos, pos) ~= '"' then error("object key expected at " .. pos) end
        local key = parse_string()
        skip()
        if str:sub(pos, pos) ~= ":" then error("':' expected at " .. pos) end
        pos = pos + 1
        obj[key] = parse_value()
        skip()
        local d = str:sub(pos, pos)
        pos = pos + 1
        if d == "}" then break end
        if d ~= "," then error("',' or '}' expected at " .. (pos - 1)) end
      end
      return obj
    elseif c == "[" then
      pos = pos + 1
      local arr = {}
      skip()
      if str:sub(pos, pos) == "]" then
        pos = pos + 1
        return arr
      end
      while true do
        arr[#arr + 1] = parse_value()
        skip()
        local d = str:sub(pos, pos)
        pos = pos + 1
        if d == "]" then break end
        if d ~= "," then error("',' or ']' expected at " .. (pos - 1)) end
      end
      return arr
    elseif c == '"' then
      return parse_string()
    elseif str:sub(pos, pos + 3) == "true" then
      pos = pos + 4
      return true
    elseif str:sub(pos, pos + 4) == "false" then
      pos = pos + 5
      return false
    elseif str:sub(pos, pos + 3) == "null" then
      pos = pos + 4
      return NULL
    else
      local num = str:match("^-?%d+%.?%d*[eE]?[+-]?%d*", pos)
      if not num or num == "" then error("unexpected character at " .. pos) end
      pos = pos + #num
      return tonumber(num)
    end
  end

  local result = parse_value()
  return result
end

local function str_of(v)
  if type(v) == "string" then return v end
  return ""
end

local function clean_title(s)
  s = s:gsub("&nbsp;", " "):gsub("&amp;", "&"):gsub("&#39;", "'"):gsub("&quot;", '"')
  return s
end

-- Cuts a text into lines of at most "width" characters.
local function wrap(text, width, max_lines)
  local lines, current = {}, ""
  -- Words longer than a line (like web links) are cut after a "/" or anywhere.
  local words = {}
  for word in text:gmatch("%S+") do
    while #word > width do
      local cut = width
      local slash = word:sub(1, width):match(".*()/")
      if slash ~= nil and slash > width / 2 then cut = slash end
      words[#words + 1] = word:sub(1, cut)
      word = word:sub(cut + 1)
    end
    words[#words + 1] = word
  end
  for _, word in ipairs(words) do
    if #current + #word + 1 > width and current ~= "" then
      lines[#lines + 1] = current
      current = word
      if #lines >= max_lines then break end
    else
      current = (current == "") and word or (current .. " " .. word)
    end
  end
  if current ~= "" and #lines < max_lines then lines[#lines + 1] = current end
  return lines
end

---------------------------------------------------------------------------
-- Game list
-- kind = "direct"   : downloadable from the Vita
--        "external" : only for PC for now (itch.io or the game's site)
--        "soon"     : not released yet
--        "local"    : a .solarus file copied by hand, unknown to the index
---------------------------------------------------------------------------
local games = {}
local notice = ""

local function file_name_from_url(url)
  local path = url:match("^([^?#]*)") or url
  local base = path:match("([^/]+)$")
  if not base or not base:lower():match("%.solarus$") then
    return nil
  end
  return (base:gsub("[^%w%._%-]", "_"))
end

local function is_blocked(name)
  local lower = name:lower()
  for _, word in ipairs(blocked_words) do
    if lower:find(word, 1, true) then return true end
  end
  return false
end

local function installed(game)
  return sol.main.vita_file_size(QUESTS_DIR .. game.file) ~= nil
end

local kind_order = { direct = 1, external = 2, soon = 3 }

local function sort_games(list)
  for _, game in ipairs(list) do
    game.is_installed = installed(game)
  end
  table.sort(list, function(a, b)
    if a.is_installed ~= b.is_installed then
      return a.is_installed
    end
    local ka, kb = kind_order[a.kind] or 0, kind_order[b.kind] or 0
    if not a.is_installed and ka ~= kb then
      return ka < kb
    end
    return a.name:lower() < b.name:lower()
  end)
end

-- Adds the .solarus files copied by hand that are not in the list.
local function add_local_games(list)
  local files = sol.main.vita_list_dir(QUESTS_DIR)
  if files == nil then return end
  local known = {}
  for _, game in ipairs(list) do
    known[game.file:lower()] = true
  end
  for _, file in ipairs(files) do
    if file:lower():match("%.solarus$") and not known[file:lower()] and not is_blocked(file) then
      local name = file:gsub("%.solarus$", ""):gsub("[%._%-]", " ")
      list[#list + 1] = {
        kind = "local", name = name, file = file, url = "", size = 0,
        excerpt = T.added_by_hand, thumbnail = "", slug = "",
      }
    end
  end
end

local function list_of(v)
  if type(v) ~= "table" then return {} end
  local result = {}
  for _, x in ipairs(v) do
    if type(x) == "string" then result[#result + 1] = clean_title(x) end
  end
  return result
end

-- Information shown by the "more info" screen.
local function details_of(g)
  local players = ""
  local min_p, max_p = tonumber(g.minimumPlayers), tonumber(g.maximumPlayers)
  if min_p and max_p and max_p > min_p then
    players = string.format(T.players_range, min_p, max_p)
  elseif min_p or max_p then
    players = tostring(min_p or max_p)
  end
  return {
    developer = table.concat(list_of(g.developer), ", "),
    genre = list_of(g.genre),
    date = str_of(g.initialReleaseDate),
    version = str_of(g.version),
    players = players,
    languages = list_of(g.languages),
    platforms = list_of(g.platforms),
  }
end

local function load_index()
  local content = sol.main.vita_read_file(INDEX_PATH)
  if not content then
    return false, "cannot read the game list"
  end
  local ok, data = pcall(decode, content)
  if not ok or type(data) ~= "table" or type(data.data) ~= "table" then
    return false, "invalid game list"
  end
  local list = {}
  for _, g in ipairs(data.data) do
    if type(g) == "table" then
      local id = str_of(g.id):gsub("[^%w%._%-]", "_")
      local url = str_of(g.download)
      local file = (url ~= "") and file_name_from_url(url) or nil
      local has_gamepad = (type(g.controls) ~= "table")  -- unknown: allow
      if type(g.controls) == "table" then
        for _, c in ipairs(g.controls) do
          if c == "gamepad" then has_gamepad = true end
        end
      end
      local thumbnail = str_of(g.thumbnail)
      local slug = thumbnail:match("^/games/([^/]+)/") or id
      local kind
      if file then
        kind = "direct"
      elseif str_of(g.version) == "" then
        kind = "soon"
      else
        kind = "external"
        file = slug .. ".solarus"   -- name to give to the file copied by hand
      end
      if file and id ~= "" and has_gamepad and not is_blocked(file) and not is_blocked(id) then
        list[#list + 1] = {
          kind = kind,
          name = str_of(g.title) ~= "" and clean_title(str_of(g.title)) or file,
          url = url,
          file = file or (id .. ".solarus"),
          size = type(g.downloadSize) == "number" and g.downloadSize or 0,
          excerpt = clean_title(str_of(g.excerpt)),
          thumbnail = thumbnail,
          slug = slug,
          website = str_of(g.website),
          id = id,
          details = details_of(g),
        }
      elseif kind == "soon" and id ~= "" and has_gamepad then
        list[#list + 1] = {
          kind = kind,
          name = clean_title(str_of(g.title)),
          url = "", file = id .. ".solarus", size = 0,
          excerpt = clean_title(str_of(g.excerpt)),
          thumbnail = thumbnail, slug = slug, website = str_of(g.website),
          id = id, details = details_of(g),
        }
      end
    end
  end
  if #list == 0 then
    return false, "no game found"
  end
  add_local_games(list)
  sort_games(list)
  games = list
  return true
end

---------------------------------------------------------------------------
-- UI: a grid of game pictures, 2 per row, like on solarus-games.org.
-- Buttons: the d-pad moves the selection, Croix plays.
-- Touch screen: slide to scroll, touch a game to select it, touch it again
-- to play.
---------------------------------------------------------------------------
local SITE_URL = "https://www.solarus-games.org"
local APP_THUMBS_DIR = "app0:thumbs/"                -- pictures shipped in the VPK
local APP_SHOTS_DIR = "app0:shots/"                  -- screenshots shipped in the VPK
local THUMBS_DIR = "ux0:data/Solarus/thumbs/"        -- downloaded pictures
local CARD_W, CARD_H = 264, 136                      -- pictures in the list (same shape as on the site)
local SPLASH_W, SPLASH_H = 320, 165                  -- pictures for the loading screen

local SCREEN_W, SCREEN_H = 960, 544
local COLS = 3
local GAP = 24
local MARGIN_X = math.floor((SCREEN_W - (COLS * CARD_W + (COLS - 1) * GAP)) / 2)
local STEP_Y = CARD_H + GAP
local HEADER_H = 56                 -- top bar
local FOOTER_Y = 464                -- bottom bar
local GRID_TOP = HEADER_H + 20      -- y of the first row (without scrolling)

local YELLOW = { 251, 184, 28 }
local BG_COLOR = { 34, 22, 70 }
local BAR_COLOR = { 22, 14, 46 }

local function new_text(size, color, align)
  return sol.text_surface.create{
    font = "dejavu",
    font_size = size,
    color = color or { 255, 255, 255 },
    horizontal_alignment = align or "left",
  }
end

local menu = {}

local background = sol.surface.create(SCREEN_W, SCREEN_H)
background:fill_color(BG_COLOR)
local header = sol.surface.create(SCREEN_W, HEADER_H)
header:fill_color(BAR_COLOR)
local footer = sol.surface.create(SCREEN_W, SCREEN_H - FOOTER_Y)
footer:fill_color(BAR_COLOR)
local line = sol.surface.create(SCREEN_W, 2)
line:fill_color{ 70, 50, 130 }
local progress_bar = sol.surface.create(SCREEN_W, 6)
progress_bar:fill_color(YELLOW)
local frame = sol.surface.create(CARD_W + 8, CARD_H + 8)
frame:fill_color(YELLOW)
local placeholder = sol.surface.create(CARD_W, CARD_H)
placeholder:fill_color{ 60, 40, 110 }
local dot_on = sol.surface.create(8, 8)
dot_on:fill_color(YELLOW)
local dot_off = sol.surface.create(8, 8)
dot_off:fill_color{ 90, 70, 150 }
local shade = sol.surface.create(SCREEN_W, SCREEN_H)
shade:fill_color{ 0, 0, 0, 170 }
local panel = sol.surface.create(760, 240)
panel:fill_color(BAR_COLOR)
local panel_frame = sol.surface.create(766, 246)
panel_frame:fill_color(YELLOW)

local logo_text = new_text(26, YELLOW, "left")
logo_text:set_text("SOLARUS")
local notice_text = new_text(16, { 170, 170, 200 }, "right")
local name_text = new_text(22, { 255, 255, 255 }, "left")
local status_text = new_text(18, YELLOW, "right")
local excerpt_text = new_text(15, { 190, 190, 220 }, "left")
local hint_text = new_text(15, { 150, 150, 185 }, "center")
local message_text = new_text(26, YELLOW, "center")

-- Little labels on the pictures.
local badges = {}
local function make_badge(key, text, color)
  local t = new_text(14, color, "center")
  t:set_text(text)
  local w = t:get_size() + 16
  local bg = sol.surface.create(w, 24)
  bg:fill_color{ 16, 10, 34, 225 }
  badges[key] = { text = t, bg = bg, w = w }
end
make_badge("installed", T.installed, YELLOW)
make_badge("later", T.on_pc, { 230, 230, 255 })
make_badge("soon", T.soon, { 170, 170, 200 })

local selected = 1
local state = "loading"   -- "loading", "list", "downloading", "launching", "error", "info"
local message = ""
local progress = 0        -- download progress, 0 to 1
local info_lines = {}     -- text of the info window
local info_surfaces = {}

-- Scrolling (in pixels).
local scroll = 0
local scroll_target = 0
local velocity = 0        -- after a slide, the list keeps going a bit
local dragging = false
local drag_start_y, drag_start_scroll, drag_last_y, drag_dy, drag_moved = 0, 0, 0, 0, 0

local function format_mb(bytes)
  return string.format("%.1f", bytes / (1024 * 1024))
end

local function base_name(game)
  return (game.file:gsub("%.solarus$", ""))
end

local function max_scroll()
  local rows = math.ceil(#games / COLS)
  local content_bottom = GRID_TOP + rows * STEP_Y - GAP + 24
  return math.max(0, content_bottom - FOOTER_Y)
end

local function clamp_scroll(v)
  return math.max(0, math.min(max_scroll(), v))
end

-- Scrolls so that the selected game is fully visible.
local function show_selected()
  local row = math.floor((selected - 1) / COLS)
  local top = GRID_TOP + row * STEP_Y
  local bottom = top + CARD_H
  if top - scroll_target < HEADER_H + 16 then
    scroll_target = top - HEADER_H - 16
  elseif bottom - scroll_target > FOOTER_Y - 16 then
    scroll_target = bottom - FOOTER_Y + 16
  end
  scroll_target = clamp_scroll(scroll_target)
  velocity = 0
end

---------------------------------------------------------------------------
-- Pictures: shipped in the VPK, or downloaded one by one in the background.
---------------------------------------------------------------------------
local thumb_queue = {}
local thumb_current = nil   -- game whose picture is downloading

-- Path of the picture of a game, or nil.
local function thumb_source(game)
  if game.slug == nil or game.slug == "" then
    return nil
  end
  local shipped = APP_THUMBS_DIR .. game.slug .. ".png"
  if sol.main.vita_file_size(shipped) ~= nil then
    return shipped
  end
  local downloaded = THUMBS_DIR .. game.slug .. ".png"
  if sol.main.vita_file_size(downloaded) ~= nil then
    return downloaded
  end
  return nil
end

local function load_thumb(game)
  if game.thumb_surface == nil and not game.thumb_missing then
    local path = thumb_source(game)
    if path ~= nil then
      game.thumb_surface = sol.main.vita_load_image(path)
    end
    if game.thumb_surface == nil then
      game.thumb_missing = true
    else
      local w = game.thumb_surface:get_size()
      game.thumb_scale = CARD_W / w
      if w ~= CARD_W then
        game.thumb_surface:set_scale(game.thumb_scale, game.thumb_scale)
      end
    end
  end
end

local function queue_thumbnails()
  sol.main.vita_make_dir("ux0:data/Solarus")
  sol.main.vita_make_dir(THUMBS_DIR)
  thumb_queue = {}
  for _, game in ipairs(games) do
    if game.thumbnail ~= nil and game.thumbnail ~= "" and thumb_source(game) == nil then
      thumb_queue[#thumb_queue + 1] = game
    end
  end
end

local function next_thumbnail()
  if thumb_current ~= nil or #thumb_queue == 0 then
    return
  end
  local game = table.remove(thumb_queue, 1)
  local url = game.thumbnail
  if url:sub(1, 1) == "/" then
    url = SITE_URL .. url
  end
  if sol.main.vita_download_start(url, THUMBS_DIR .. game.slug .. ".dl") then
    thumb_current = game
  end
end

local function thumbnail_downloaded()
  local game = thumb_current
  thumb_current = nil
  local raw = THUMBS_DIR .. game.slug .. ".dl"
  sol.main.vita_make_thumbnail(raw, THUMBS_DIR .. game.slug .. ".png", CARD_W, CARD_H)
  sol.main.vita_remove_file(raw)
  game.thumb_surface = nil
  game.thumb_missing = false
end

-- Small picture shown by the loading screen of the game.
local function make_splash_picture(game)
  local dst = THUMBS_DIR .. base_name(game) .. ".png"
  if sol.main.vita_file_size(dst) ~= nil then
    return
  end
  local src = thumb_source(game)
  if src ~= nil then
    sol.main.vita_make_dir(THUMBS_DIR)
    sol.main.vita_make_thumbnail(src, dst, SPLASH_W, SPLASH_H)
  end
end

---------------------------------------------------------------------------
-- Actions
---------------------------------------------------------------------------
local function use_fallback(reason)
  games = fallback_games
  for _, game in ipairs(games) do
    game.kind = game.kind or "direct"
  end
  add_local_games(games)
  sort_games(games)
  notice = reason
  state = "list"
  selected = 1
end

local function finish_loading(reason)
  local ok, err = load_index()
  if ok then
    notice = reason or ""
    state = "list"
    selected = 1
    queue_thumbnails()
  else
    use_fallback(err)
  end
end

local function start_game(game)
  state = "launching"
  message = string.format(T.loading_game, game.name)
  make_splash_picture(game)
  -- Let one frame be drawn before the app restarts.
  sol.timer.start(sol.main, 200, function()
    local ok = sol.main.vita_launch(QUESTS_DIR .. game.file)
    if not ok then
      state = "error"
      message = T.cannot_launch
    end
  end)
end

-- Small window with a few lines of text (state "info" or "confirm").
local function show_panel(lines, new_state)
  info_lines = lines
  info_surfaces = {}
  for i, l in ipairs(lines) do
    local t = new_text(i == 1 and 24 or 18, i == 1 and YELLOW or { 230, 230, 255 }, "center")
    t:set_text(l)
    info_surfaces[i] = t
  end
  state = new_state or "info"
end

local function show_info(lines)
  show_panel(lines, "info")
end

---------------------------------------------------------------------------
-- "More info" screen (Triangle).
---------------------------------------------------------------------------
-- French descriptions taken from solarus-games.org (data/infos_fr.lua).
local infos_fr = {}
do
  local ok, chunk = pcall(sol.main.load_file, "infos_fr.lua")
  if ok and chunk ~= nil then
    local ok2, result = pcall(chunk)
    if ok2 and type(result) == "table" then
      infos_fr = result
    end
  end
end

local platform_names = {
  windows = "Windows", macos = "macOS", linux = "Linux", android = "Android", ios = "iOS",
  ["nintendo-switch"] = "Nintendo Switch", web = "Web", ["steam-deck"] = "Steam Deck",
  playstation = "PlayStation", xbox = "Xbox",
}
local genre_names = T.genres
local language_names = T.languages

local function translate_list(list, names)
  local result = {}
  for _, x in ipairs(list or {}) do
    result[#result + 1] = names[x] or x
  end
  return table.concat(result, ", ")
end

local function format_date(date)
  local y, m, d = date:match("^(%d+)-(%d+)-(%d+)")
  if y then return d .. "/" .. m .. "/" .. y end
  return date
end

local function status_of(game)
  if game.is_installed then
    return T.installed
  elseif game.kind == "soon" then
    return T.soon
  elseif game.kind == "external" then
    return T.on_pc
  elseif game.size > 0 then
    return T.to_download .. " (" .. format_mb(game.size) .. " " .. T.mb .. ")"
  end
  return T.to_download
end

-- Description of a game, in French when the console is in French.
local function description_of(game)
  if lang == "fr" then
    local info = infos_fr[game.slug or ""]
    if info ~= nil and info.description ~= nil and info.description ~= "" then
      return info.description
    end
  end
  return game.excerpt or ""
end

local details = nil   -- surfaces of the "more info" screen
local DETAILS_TEXT_BOTTOM = FOOTER_Y - 12
local DETAILS_LINE_H = 24

local function open_details(game)
  details = { game = game, title = {}, fields = {}, text = {}, first_line = 1 }
  -- Pictures shipped in the VPK (330x170): the game's picture, then a few
  -- screenshots, shown one after the other.
  details.pictures = {}
  if game.slug ~= nil and game.slug ~= "" then
    local cover = sol.main.vita_load_image(APP_THUMBS_DIR .. "big/" .. game.slug .. ".png")
    if cover ~= nil then
      details.pictures[#details.pictures + 1] = cover
    end
    for n = 1, 3 do
      local shot = sol.main.vita_load_image(APP_SHOTS_DIR .. game.slug .. "-" .. n .. ".png")
      if shot ~= nil then
        details.pictures[#details.pictures + 1] = shot
      end
    end
  end
  details.picture_index = 1
  details.picture_time = sol.main.get_elapsed_time()
  details.previous_index = nil
  details.fade_start = 0
  for _, l in ipairs(wrap(game.name, 34, 2)) do
    local t = new_text(24, YELLOW, "left")
    t:set_text(l)
    details.title[#details.title + 1] = t
  end
  local d = game.details or {}
  local fields = {
    { T.f_status, status_of(game) },
    { T.f_developer, d.developer },
    { T.f_genre, translate_list(d.genre, genre_names) },
    { T.f_date, d.date and format_date(d.date) or "" },
    { T.f_version, d.version },
    { T.f_players, d.players },
    { T.f_languages, translate_list(d.languages, language_names) },
    { T.f_platforms, translate_list(d.platforms, platform_names) },
  }
  for _, f in ipairs(fields) do
    if f[2] ~= nil and f[2] ~= "" then
      local t = new_text(15, { 230, 230, 255 }, "left")
      local field = f[1] .. " : " .. f[2]
      if #field > 42 then field = field:sub(1, 40) .. ".." end
      t:set_text(field)
      details.fields[#details.fields + 1] = t
    end
  end
  local description = description_of(game)
  local paragraphs = {}
  for paragraph in (description .. "\n"):gmatch("(.-)\n") do
    paragraphs[#paragraphs + 1] = paragraph
  end
  if game.slug ~= nil and game.slug ~= "" then
    paragraphs[#paragraphs + 1] = "#" .. T.solarus_page
    paragraphs[#paragraphs + 1] = T.solarus_page_url .. game.slug
  end
  -- The game's own website, when it is not the Solarus page (itch.io, ...).
  local site = (game.website or ""):gsub("^https?://", ""):gsub("/$", "")
  if site ~= "" and not site:find("solarus-games.org", 1, true) then
    paragraphs[#paragraphs + 1] = "#" .. T.game_site
    paragraphs[#paragraphs + 1] = site
  end
  for _, paragraph in ipairs(paragraphs) do
    -- A line starting with "#" is a small title.
    local is_title = paragraph:sub(1, 1) == "#"
    if is_title then
      paragraph = paragraph:sub(2)
      if #details.text > 0 then
        local blank = new_text(16, { 200, 200, 225 }, "left")
        details.text[#details.text + 1] = blank
      end
    end
    local lines = wrap(paragraph, 52, 200)
    if #lines == 0 then lines = { "" } end
    for _, l in ipairs(lines) do
      local t = new_text(16, is_title and YELLOW or { 200, 200, 225 }, "left")
      t:set_text(l)
      details.text[#details.text + 1] = t
    end
  end
  -- The text is on the right, below the title.
  details.text_top = 92 + #details.title * 30 + 12
  details.visible_lines = math.max(1, math.floor((DETAILS_TEXT_BOTTOM - details.text_top) / DETAILS_LINE_H))
  state = "details"
end

-- Shows the next (delta = 1) or previous (delta = -1) picture.
local function change_picture(delta)
  if details == nil or #details.pictures < 2 then return end
  local n = #details.pictures
  details.previous_index = details.picture_index
  details.picture_index = ((details.picture_index - 1 + delta) % n) + 1
  details.fade_start = sol.main.get_elapsed_time()
  details.picture_time = details.fade_start
end

local function scroll_details(delta)
  if details == nil then return end
  local max_first = math.max(1, #details.text - details.visible_lines + 1)
  details.first_line = math.max(1, math.min(max_first, details.first_line + delta))
end

---------------------------------------------------------------------------
-- Uninstalling a game (Rond).
---------------------------------------------------------------------------
local confirm = nil   -- { game = ..., step = 1 or 2 }

local function saves_dir(game)
  if game.id == nil or game.id == "" then return nil end
  return sol.main.vita_get_saves_dir() .. "/" .. game.id
end

local function has_saves(game)
  local dir = saves_dir(game)
  return dir ~= nil and sol.main.vita_list_dir(dir) ~= nil
end

local function ask_uninstall(game)
  confirm = { game = game, step = 1 }
  show_panel({
    T.uninstall_question,
    game.name,
    "",
    T.yes_no,
  }, "confirm")
end

local function uninstall(game, with_saves)
  sol.main.vita_remove_file(QUESTS_DIR .. game.file)
  sol.main.vita_remove_file(THUMBS_DIR .. base_name(game) .. ".png")
  local saves_text = ""
  if with_saves then
    sol.main.vita_remove_dir(saves_dir(game))
    sol.main.vita_remove_file("ux0:data/Solarus/keymaps/" .. game.id .. ".txt")
    saves_text = T.saves_deleted
  elseif has_saves(game) then
    saves_text = T.saves_kept
  end
  game.is_installed = installed(game)
  if game.is_installed then
    show_info{ game.name, "", T.cannot_delete }
    return
  end
  if game.kind == "local" then
    table.remove(games, selected)
    selected = math.max(1, math.min(selected, #games))
  end
  scroll_target = clamp_scroll(scroll_target)
  show_info{ game.name, "", T.uninstalled, saves_text }
end

local function confirm_answer(yes)
  local game = confirm.game
  if confirm.step == 1 then
    if not yes then
      confirm = nil
      state = "list"
    elseif has_saves(game) then
      confirm.step = 2
      show_panel({
        T.saves_question,
        T.saves_keep_1,
        T.saves_keep_2,
        "",
        T.delete_keep,
      }, "confirm")
    else
      confirm = nil
      uninstall(game, false)
    end
  else
    confirm = nil
    uninstall(game, yes)
  end
end

---------------------------------------------------------------------------
-- Croix: play, download, or more info.
---------------------------------------------------------------------------
local function activate()
  local game = games[selected]
  if not game then return end
  if installed(game) then
    start_game(game)
  elseif game.kind == "direct" then
    if thumb_current ~= nil then
      -- A picture was downloading: it will be fetched again later.
      table.insert(thumb_queue, 1, thumb_current)
      thumb_current = nil
    end
    sol.main.vita_make_dir("ux0:data/Solarus")
    sol.main.vita_make_dir(QUESTS_DIR)
    if sol.main.vita_download_start(game.url, QUESTS_DIR .. game.file) then
      state = "downloading"
      message = T.downloading
      progress = 0
    else
      state = "error"
      message = T.cannot_download
    end
  else
    open_details(game)
  end
end

local function select_game(index)
  if index < 1 or index > #games then return end
  selected = index
  show_selected()
end

---------------------------------------------------------------------------
-- Input: buttons
---------------------------------------------------------------------------
local last_move = -1000

local directions = {
  dpad_left = -1, left = -1,
  dpad_right = 1, right = 1,
  dpad_up = -COLS, up = -COLS,
  dpad_down = COLS, down = COLS,
}

local function is_cross(name) return name == "a" or name == "return" or name == "space" end
local function is_circle(name) return name == "b" or name == "c" or name == "escape" end
local function is_triangle(name) return name == "y" or name == "i" end

-- The d-pad can send the same press twice (button + arrow key).
local function move_allowed()
  local now = sol.main.get_elapsed_time()
  if now - last_move < 150 then
    return false
  end
  last_move = now
  return true
end

local function handle_input(name)
  if state == "loading" or state == "launching" then
    return true
  end
  if state == "downloading" then
    if is_circle(name) then
      sol.main.vita_download_cancel()
      state = "list"
    end
    return true
  end
  if state == "error" or state == "info" then
    state = "list"
    return true
  end
  if state == "confirm" then
    if is_cross(name) then
      confirm_answer(true)
    elseif is_circle(name) then
      confirm_answer(false)
    end
    return true
  end
  if state == "details" then
    if is_circle(name) or is_triangle(name) then
      state = "list"
    elseif is_cross(name) then
      local game = details.game
      if game.is_installed or game.kind == "direct" then
        activate()
      end
    elseif (name == "dpad_left" or name == "left") and move_allowed() then
      change_picture(-1)
    elseif (name == "dpad_right" or name == "right") and move_allowed() then
      change_picture(1)
    elseif (name == "dpad_up" or name == "up") and move_allowed() then
      scroll_details(-3)
    elseif (name == "dpad_down" or name == "down") and move_allowed() then
      scroll_details(3)
    end
    return true
  end
  local delta = directions[name]
  if delta ~= nil then
    if not move_allowed() then
      return true
    end
    local target = selected + delta
    if delta == COLS and target > #games then
      -- Going down from the row above the last one.
      if math.floor((selected - 1) / COLS) < math.floor((#games - 1) / COLS) then
        target = #games
      end
    end
    select_game(target)
  elseif is_cross(name) then
    activate()
  elseif is_triangle(name) then
    if games[selected] then open_details(games[selected]) end
  elseif is_circle(name) then
    local game = games[selected]
    if game and game.is_installed then
      ask_uninstall(game)
    end
  else
    return false
  end
  return true
end

function menu:on_joypad_button_pressed(...)
  for _, v in ipairs({ ... }) do
    local name = v
    if type(v) == "number" then name = button_names[v] end
    if type(name) == "string" and handle_input(name) then
      return true
    end
  end
  return false
end

function menu:on_key_pressed(key)
  return handle_input(key)
end

---------------------------------------------------------------------------
-- Input: touch screen
---------------------------------------------------------------------------
-- Returns the index of the game at this screen position, or nil.
local function game_at(x, y)
  if y < HEADER_H or y >= FOOTER_Y then return nil end
  local gx = x - MARGIN_X
  local gy = y + scroll - GRID_TOP
  if gx < 0 or gy < 0 then return nil end
  local col = math.floor(gx / (CARD_W + GAP))
  local row = math.floor(gy / STEP_Y)
  if col >= COLS or gx - col * (CARD_W + GAP) > CARD_W or gy - row * STEP_Y > CARD_H then
    return nil
  end
  local index = row * COLS + col + 1
  if index > #games then return nil end
  return index
end

function menu:on_finger_pressed(finger, x, y)
  if state ~= "list" then
    -- Touching the screen closes a message or the "more info" screen
    -- (but never answers a question).
    if state == "details" and details ~= nil and x >= 60 and x < 60 + 330 and y >= 76 and y < 76 + 170 then
      change_picture(1)   -- Touching the picture shows the next one.
    elseif state == "error" or state == "info" or state == "details" then
      state = "list"
    end
    return true
  end
  dragging = true
  velocity = 0
  drag_start_y, drag_last_y, drag_dy, drag_moved = y, y, 0, 0
  drag_start_scroll = scroll
  return true
end

function menu:on_finger_moved(finger, x, y)
  if not dragging then return false end
  drag_dy = y - drag_last_y
  drag_last_y = y
  drag_moved = math.max(drag_moved, math.abs(y - drag_start_y))
  if drag_moved > 16 then
    scroll = clamp_scroll(drag_start_scroll - (y - drag_start_y))
    scroll_target = scroll
  end
  return true
end

function menu:on_finger_released(finger, x, y)
  if not dragging then return false end
  dragging = false
  if drag_moved > 16 then
    -- A slide: the list keeps going a bit.
    velocity = -drag_dy * 0.6
  else
    -- A touch: select the game, or play it if it was already selected.
    local index = game_at(x, y)
    if index ~= nil then
      if index == selected then
        activate()
      else
        select_game(index)
      end
    end
  end
  return true
end

---------------------------------------------------------------------------
-- Update
---------------------------------------------------------------------------
local function update_scroll()
  if dragging then return end
  if velocity ~= 0 then
    scroll = clamp_scroll(scroll + velocity)
    scroll_target = scroll
    velocity = velocity * 0.94
    if math.abs(velocity) < 0.5 then velocity = 0 end
  elseif scroll ~= scroll_target then
    scroll = scroll + (scroll_target - scroll) * 0.25
    if math.abs(scroll_target - scroll) < 0.5 then scroll = scroll_target end
  end
end

local PICTURE_DELAY = 3500   -- time each picture stays on the "more info" screen
local FADE_DURATION = 400

function menu:on_update()
  update_scroll()

  if state == "details" and details ~= nil and #details.pictures > 1 then
    if sol.main.get_elapsed_time() - details.picture_time > PICTURE_DELAY then
      change_picture(1)
    end
  end

  if state == "loading" then
    local st = sol.main.vita_download_poll()
    if st.state == "running" then
      message = T.loading_list
    elseif st.state == "done" then
      finish_loading()
    elseif st.state == "error" then
      -- Show why, to make bug reports easier.
      local reason = st.error or "?"
      if #reason > 60 then
        reason = reason:sub(1, 57) .. "..."
      end
      if sol.main.vita_file_size(INDEX_PATH) then
        finish_loading(T.offline_saved .. " (" .. reason .. ")")
      else
        use_fallback(T.offline .. " (" .. reason .. ")")
      end
    end
    return
  end

  if state == "downloading" then
    local st = sol.main.vita_download_poll()
    if st.state == "running" then
      if st.total > 0 then
        progress = st.bytes / st.total
        message = string.format("%s / %s %s (%d%%)",
            format_mb(st.bytes), format_mb(st.total), T.mb, math.floor(progress * 100))
      else
        message = format_mb(st.bytes) .. " " .. T.mb
      end
    elseif st.state == "done" then
      state = "list"
      start_game(games[selected])
    elseif st.state == "error" then
      state = "error"
      message = T.download_failed .. (st.error or "?")
    end
    return
  end

  if state == "list" or state == "info" then
    if thumb_current ~= nil then
      local st = sol.main.vita_download_poll()
      if st.state == "done" then
        thumbnail_downloaded()
      elseif st.state == "error" then
        thumb_current = nil   -- No picture for this game.
      end
    else
      next_thumbnail()
    end
  end
end

---------------------------------------------------------------------------
-- Drawing
---------------------------------------------------------------------------

-- Name written on the card while its picture is missing.
local function draw_label(dst, game, x, y)
  if game.label_surfaces == nil then
    game.label_surfaces = {}
    for _, l in ipairs(wrap(game.name, 24, 3)) do
      local t = new_text(17, { 230, 230, 255 }, "center")
      t:set_text(l)
      game.label_surfaces[#game.label_surfaces + 1] = t
    end
  end
  local n = #game.label_surfaces
  local ly = y + CARD_H / 2 - (n - 1) * 11
  for _, t in ipairs(game.label_surfaces) do
    t:draw(dst, x + CARD_W / 2, ly)
    ly = ly + 22
  end
end

local function badge_of(game)
  if game.is_installed then
    return badges.installed
  elseif game.kind == "soon" then
    return badges.soon
  elseif game.kind == "external" then
    return badges.later
  end
  return nil
end

local function draw_card(dst, index, x, y)
  local game = games[index]
  if index == selected then
    frame:draw(dst, x - 4, y - 4)
  end
  load_thumb(game)
  if game.thumb_surface ~= nil then
    game.thumb_surface:draw(dst, x, y)
  else
    placeholder:draw(dst, x, y)
    draw_label(dst, game, x, y)
  end
  local b = badge_of(game)
  if b ~= nil then
    local bx, by = x + CARD_W - b.w - 8, y + CARD_H - 32
    b.bg:draw(dst, bx, by)
    b.text:draw(dst, bx + b.w / 2, by + 12)
  end
end

local function footer_texts()
  local game = games[selected]
  if game == nil then return "", "", "", "" end
  local name = game.name
  if #name > 50 then name = name:sub(1, 48) .. ".." end
  -- First paragraph of the description (not a small title).
  local first = ""
  for paragraph in (description_of(game) .. "\n"):gmatch("(.-)\n") do
    if paragraph ~= "" and paragraph:sub(1, 1) ~= "#" then
      first = paragraph
      break
    end
  end
  local excerpt = wrap(first, 104, 2)
  local excerpt_line = excerpt[1] or ""
  if excerpt[2] ~= nil then excerpt_line = excerpt_line .. "..." end
  if state == "downloading" then
    return name, message, excerpt_line, T.hint_cancel
  elseif state == "error" then
    return message, "", "", T.press_button
  elseif game.is_installed then
    return name, status_of(game), excerpt_line, T.hint_installed
  elseif game.kind == "direct" then
    return name, status_of(game), excerpt_line, T.hint_download
  else
    return name, status_of(game), excerpt_line, T.hint_info
  end
end

local DETAILS_PICTURE_SCALE = 1.25

local function draw_details(dst)
  local game = details.game
  -- Picture.
  local px, py = 60, 76
  local pw, ph = math.floor(CARD_W * DETAILS_PICTURE_SCALE), math.floor(CARD_H * DETAILS_PICTURE_SCALE)
  load_thumb(game)
  if #details.pictures > 0 then
    local current = details.pictures[details.picture_index]
    local t = (sol.main.get_elapsed_time() - details.fade_start) / FADE_DURATION
    if details.previous_index ~= nil and t < 1 then
      -- Fade from the previous picture to the new one.
      details.pictures[details.previous_index]:draw(dst, px, py)
      current:set_opacity(math.floor(255 * math.max(0, t)))
      current:draw(dst, px, py)
      current:set_opacity(255)
    else
      current:draw(dst, px, py)
    end
    -- Small squares: which picture is shown.
    if #details.pictures > 1 then
      local n = #details.pictures
      local dx = px + math.floor((pw - (n * 14 - 6)) / 2)
      for i = 1, n do
        local dot = (i == details.picture_index) and dot_on or dot_off
        dot:draw(dst, dx + (i - 1) * 14, py + ph + 8)
      end
    end
  elseif game.thumb_surface ~= nil then
    local scale = (game.thumb_scale or 1) * DETAILS_PICTURE_SCALE
    game.thumb_surface:set_scale(scale, scale)
    game.thumb_surface:draw(dst, px, py)
    game.thumb_surface:set_scale(game.thumb_scale or 1, game.thumb_scale or 1)
  else
    placeholder:set_scale(DETAILS_PICTURE_SCALE, DETAILS_PICTURE_SCALE)
    placeholder:draw(dst, px, py)
    placeholder:set_scale(1, 1)
  end
  -- Fields, below the picture.
  local y = py + ph + 32
  for _, t in ipairs(details.fields) do
    t:draw(dst, px, y)
    y = y + 21
  end
  -- Title and description, on the right of the picture.
  local x = px + pw + 30
  y = 92
  for _, t in ipairs(details.title) do
    t:draw(dst, x, y)
    y = y + 30
  end
  local ty = details.text_top
  local last = math.min(#details.text, details.first_line + details.visible_lines - 1)
  for i = details.first_line, last do
    details.text[i]:draw(dst, x, ty)
    ty = ty + DETAILS_LINE_H
  end
  -- Bars.
  header:draw(dst, 0, 0)
  line:draw(dst, 0, HEADER_H - 2)
  logo_text:draw(dst, 60, HEADER_H / 2)
  footer:draw(dst, 0, FOOTER_Y)
  line:draw(dst, 0, FOOTER_Y)
  local hint
  if game.is_installed then
    hint = T.hint_details_play
  elseif game.kind == "direct" then
    hint = T.hint_details_download
  else
    hint = T.hint_details
  end
  hint_text:set_text(hint)
  hint_text:draw(dst, SCREEN_W / 2, FOOTER_Y + 40)
end

function menu:on_draw(dst)
  background:draw(dst, 0, 0)

  if state == "loading" or state == "launching" then
    message_text:set_text(message)
    message_text:draw(dst, SCREEN_W / 2, SCREEN_H / 2)
    return
  end

  if state == "details" and details ~= nil then
    draw_details(dst)
    return
  end

  -- Grid (only the visible rows).
  local s = math.floor(scroll + 0.5)
  for index = 1, #games do
    local row = math.floor((index - 1) / COLS)
    local col = (index - 1) % COLS
    local x = MARGIN_X + col * (CARD_W + GAP)
    local y = GRID_TOP + row * STEP_Y - s
    if y + CARD_H > HEADER_H - 8 and y < FOOTER_Y + 8 then
      draw_card(dst, index, x, y)
    end
  end

  -- Top bar.
  header:draw(dst, 0, 0)
  line:draw(dst, 0, HEADER_H - 2)
  logo_text:draw(dst, 60, HEADER_H / 2)
  if notice ~= "" then
    notice_text:set_text(notice)
  else
    notice_text:set_text(string.format(T.games_count, #games))
  end
  notice_text:draw(dst, SCREEN_W - 60, HEADER_H / 2)

  -- Bottom bar: selected game, its status and the buttons.
  footer:draw(dst, 0, FOOTER_Y)
  line:draw(dst, 0, FOOTER_Y)
  if state == "downloading" and progress > 0 then
    progress_bar:draw_region(0, 0, math.floor(SCREEN_W * progress), 6, dst, 0, FOOTER_Y)
  end
  local name, status, excerpt, hint = footer_texts()
  name_text:set_text(name)
  name_text:draw(dst, 60, FOOTER_Y + 20)
  status_text:set_text(status)
  status_text:draw(dst, SCREEN_W - 60, FOOTER_Y + 20)
  excerpt_text:set_text(excerpt)
  excerpt_text:draw(dst, 60, FOOTER_Y + 45)
  hint_text:set_text(hint)
  hint_text:draw(dst, SCREEN_W / 2, FOOTER_Y + 67)

  -- Small window: message or question.
  if state == "info" or state == "confirm" then
    shade:draw(dst, 0, 0)
    local px, py = (SCREEN_W - 760) / 2, (SCREEN_H - 240) / 2
    panel_frame:draw(dst, px - 3, py - 3)
    panel:draw(dst, px, py)
    local ty = py + 40
    for _, t in ipairs(info_surfaces) do
      t:draw(dst, SCREEN_W / 2, ty)
      ty = ty + 32
    end
    if state == "info" then
      hint_text:set_text(T.press_to_close)
      hint_text:draw(dst, SCREEN_W / 2, py + 240 - 22)
    end
  end
end

function sol.main:on_started()
  -- On a Vita that never had Solarus, nothing exists yet in ux0:data:
  -- create the folders before downloading anything into them.
  sol.main.vita_make_dir("ux0:data/Solarus")
  sol.main.vita_make_dir(QUESTS_DIR)
  sol.main.vita_make_dir(THUMBS_DIR)
  sol.main.vita_make_dir("ux0:data/Solarus/keymaps")
  sol.menu.start(sol.main, menu)
  -- Music of the game list: data/musics/menu.ogg, if present.
  if sol.main.vita_file_size("app0:data/musics/menu.ogg") ~= nil then
    sol.audio.set_music_volume(60)
    sol.audio.play_music("menu")
  end
  -- The French list has the same games, with French short descriptions.
  local url = INDEX_URL
  if lang == "fr" then
    url = "https://www.solarus-games.org/fr/games/index.json"
  end
  sol.main.vita_download_start(url, INDEX_PATH)
end
