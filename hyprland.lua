local playerClass = "omarchy-read-aloud"
local player = "'" .. os.getenv("HOME"):gsub("'", "'\\''") .. "/.local/bin/omarchy-read-aloud'"
local controls = {
  {"CTRL + DOWN", "Pause reading", "--pause"},
  {"CTRL + LEFT", "Previous spoken sentence", "--previous"},
  {"CTRL + RIGHT", "Next spoken sentence", "--next"},
}

o.bind("CTRL + UP", "Start or resume reading", player .. " --start")
o.bind("SUPER + SHIFT + R", "Read selected text", player)
o.bind("SUPER + ALT + R", "Read copied text", player .. " --clipboard")
o.window("^" .. playerClass .. "$", {
  float = true,
  pin = true,
  move = { "monitor_w - window_w - 14", 44 },
  no_initial_focus = true,
  decorate = false,
  border_size = 0,
  no_shadow = true,
  no_anim = true,
  no_blur = true,
})

local controlsActive = false
local hoverOverride = false

local function updateHover()
  local mode = hl.get_config("input.follow_mouse")
  if controlsActive and mode == 0 then
    -- Mode 2 gives the unfocused card pointer events and keeps click-to-focus.
    hl.config({ input = { follow_mouse = 2 } })
    hoverOverride = true
  elseif mode ~= 2 then
    hoverOverride = false
  elseif not controlsActive and hoverOverride then
    hl.config({ input = { follow_mouse = 0 } })
    hoverOverride = false
  end
end

local function setControls(active)
  if controlsActive == active then return end
  controlsActive = active
  updateHover()
  for _, control in ipairs(controls) do
    if active then
      o.bind(control[1], control[2], player .. " " .. control[3])
    else
      hl.unbind(control[1])
    end
  end
end

hl.on("window.open", function(window)
  if window.class == playerClass then setControls(true) end
end)
hl.on("window.close", function(window)
  if window.class == playerClass then setControls(false) end
end)
hl.on("workspace.active", function()
  -- Omarchy may update the focus mode as part of the same workspace event.
  hl.timer(updateHover, { type = "oneshot", timeout = 1 })
end)
setControls(#hl.get_windows({ class = playerClass }) > 0)
