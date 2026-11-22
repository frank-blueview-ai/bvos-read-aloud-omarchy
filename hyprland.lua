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
  no_blur = true,
})

local controlsActive = false
local function setControls(active)
  if controlsActive == active then return end
  controlsActive = active
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
setControls(#hl.get_windows({ class = playerClass }) > 0)
