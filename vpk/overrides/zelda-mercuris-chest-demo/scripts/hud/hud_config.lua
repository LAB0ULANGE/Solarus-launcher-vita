-- Defines the elements to put in the HUD
-- and their position on the game screen.

-- You can edit this file to add, remove or move some elements of the HUD.

-- Each HUD element script must provide a method new()
-- that creates the element as a menu.
-- See for example scripts/hud/hearts.lua.

-- Negative x or y coordinates mean to measure from the right or bottom
-- of the screen, respectively.

local hud_config = {

  -- Hearts meter.
  {
    menu_script = "scripts/hud/hearts",
    x = 8,
    y = 8,
  },

  -- Magic bar.
  {
    menu_script = "scripts/hud/magic_bar",
    x = 8,
    y = 20,
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
    x = 8,
    y = -36,
  },

  -- Location view
  {
    menu_script = "scripts/hud/location_label",
    x = 0,
    y = 48,
  },

  -- Out minimap
  {
    menu_script = "scripts/hud/out_minimap",
    x = -74,
    y = -74,
  },

  -- Item icon for slot 2.
  {
    menu_script = "scripts/hud/item_icon",
    controller_x = -50,
    controller_y = 6,
    keyboard_x = -50,
    keyboard_y = 6,
    slot = 2,  -- Item slot (1 or 2).
  },

  -- Item icon for slot 1.
  {
    menu_script = "scripts/hud/item_icon",
    controller_x = -69,
    controller_y = 23,
    keyboard_x = -69,
    keyboard_y = 23,
    slot = 1,  -- Item slot (1 or 2).
  },  

  -- Attack icon.
  {
    menu_script = "scripts/hud/attack_icon",
    controller_x = -55,
    controller_y = 23,
    keyboard_x = -55,
    keyboard_y = 23,
  },

  -- Action icon.
  {
    menu_script = "scripts/hud/action_icon",
    controller_x = -74,
    controller_y = 40,
    keyboard_x = -74,
    keyboard_y = 40,
  },

  -- Ending frame
  {
    menu_script = "scripts/hud/demo_ending/soon",
    x = 0,
    y = 0,
  },

  -- Ending contributors
  {
    menu_script = "scripts/hud/demo_ending/contributors",
    x = 0,
    y = 0,
  }
}

return hud_config
