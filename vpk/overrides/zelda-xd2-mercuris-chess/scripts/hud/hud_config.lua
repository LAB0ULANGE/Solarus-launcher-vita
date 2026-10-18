-- Vita version: the item, sword and action icons are placed like the
-- face buttons of the PS Vita (Triangle top, Square left, Circle right,
-- Cross bottom).

local hud_config = {

  -- Hearts meter.
  {
    menu_script = "scripts/hud/hearts",
    x = -87,
    y = 6,
  },

  -- Magic bar.
  {
    menu_script = "scripts/hud/magic_bar",
    x = -52,
    y = 27,
  },

  -- Rupee counter.
  {
    menu_script = "scripts/hud/rupees",
    x = 8,
    y = -20,
  },

  -- Small key counter.
  {
    menu_script = "scripts/hud/small_keys",
    x = -30,
    y = -19,
  },

  -- Floor view.
  {
    menu_script = "scripts/hud/floor",
    x = 6,
    y = 70,
  },

  -- Prehistoric label.
  {
    menu_script = "scripts/hud/prehistoric",
    x = -58,
    y = -22,
  },

  -- Pause icon (Start).
  {
    menu_script = "scripts/hud/pause_icon",
    x = 0,
    y = 6,
  },

  -- Item icon for slot 1 (Square, left).
  {
    menu_script = "scripts/hud/item_icon",
    x = 34,
    y = 34,
    slot = 1,
  },

  -- Item icon for slot 2 (Triangle, top).
  {
    menu_script = "scripts/hud/item_icon",
    x = 50,
    y = 18,
    slot = 2,
  },

  -- Attack icon (Circle, right).
  {
    menu_script = "scripts/hud/attack_icon",
    x = 43,
    y = 36,
  },

  -- Action icon (Cross, bottom).
  {
    menu_script = "scripts/hud/action_icon",
    x = 27,
    y = 52,
  },
}

return hud_config
