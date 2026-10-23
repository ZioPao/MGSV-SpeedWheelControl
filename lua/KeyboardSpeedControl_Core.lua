-- KeyboardSpeedControl_Core.lua
-- Infinite Heaven external module: adds the Keyboard Speed Control knobs to the IH menu (F3)
-- and publishes them to plugins/KeyboardSpeedControl.runtime, which KeyboardSpeedControl.dll polls.
--
-- Install: copy to MGS_TPP/mod/modules/KeyboardSpeedControl_Core.lua
-- IH auto-loads every .lua in that folder, so no other setup is needed.

local this = {}

if rawget(_G, "KeyboardSpeedControl_Core") then
  return _G.KeyboardSpeedControl_Core
end

local runtimeName = "KeyboardSpeedControl.runtime"
local writeSeq = 0

local function GamePath()
  local path = (InfCore and InfCore.gamePath) or (IHH and IHH.GetGamePath and IHH.GetGamePath()) or ".\\"
  local last = string.sub(path, -1)
  if last ~= "\\" and last ~= "/" then
    path = path .. "\\"
  end
  return path
end

local function RuntimePath()
  return GamePath() .. "plugins\\" .. runtimeName
end

local function IvarValue(name, default)
  local ivar = Ivars and Ivars[name]
  if ivar and ivar.Get then
    local ok, value = pcall(ivar.Get, ivar)
    if ok and value ~= nil then
      return value
    end
  end
  return default
end

local function WriteRuntime(includeLevel)
  local f = io.open(RuntimePath(), "w")
  if not f then return end
  writeSeq = writeSeq + 1
  f:write("seq=" .. tostring(writeSeq) .. "\n")
  f:write("enabled=" .. tostring(IvarValue("ksc_enabled", 1)) .. "\n")
  f:write(string.format("smooth=%.3f\n", IvarValue("ksc_smooth", 0.12)))
  f:write("invert=" .. tostring(IvarValue("ksc_invert", 0)) .. "\n")
  f:write("step=" .. tostring(IvarValue("ksc_wheelStep", 1)) .. "\n")
  if includeLevel then
    -- One-shot: jump to the top of the ladder. Normal writes omit level so the
    -- wheel stays authoritative.
    f:write("level=8\n")
  end
  f:close()
end

-- NOTE: no per-frame Update here. Calling io.open from the game's Lua update
-- deadlocks on the asset/file-system lock during loading (the game hangs at the
-- first Lua update). Writes only happen from menu callbacks, which are safe.

-- ---------------------------------------------------------------------------
-- Knobs
-- ---------------------------------------------------------------------------

local function OnChange()
  pcall(function() WriteRuntime(false) end)
end

this.registerIvars = {
  "ksc_enabled",
  "ksc_wheelStep",
  "ksc_smooth",
  "ksc_invert",
}

this.ksc_enabled = {
  description = "Speed system",
  inMission = true,
  save = IvarProc.CATEGORY_EXTERNAL,
  range = Ivars.switchRange,
  settingNames = "set_switch",
  default = 1,
  OnChange = OnChange,
}

this.ksc_wheelStep = {
  description = "Wheel step",
  inMission = true,
  save = IvarProc.CATEGORY_EXTERNAL,
  default = 1,
  range = { min = 1, max = 3, increment = 1 },
  OnChange = OnChange,
  GetSettingText = function(self, setting)
    if setting == 1 then return "1 level" end
    return string.format("%d levels", setting)
  end,
}

this.ksc_smooth = {
  description = "Transition smoothing",
  inMission = true,
  save = IvarProc.CATEGORY_EXTERNAL,
  default = 0.12,
  range = { min = 0, max = 0.50, increment = 0.02 },
  OnChange = OnChange,
  GetSettingText = function(self, setting)
    return string.format("%.2f s", setting)
  end,
}

this.ksc_invert = {
  description = "Invert wheel",
  inMission = true,
  save = IvarProc.CATEGORY_EXTERNAL,
  range = Ivars.switchRange,
  settingNames = "set_switch",
  default = 0,
  OnChange = OnChange,
}

-- Command item: jump back to vanilla run speed.
function this.ResetLevel()
  pcall(function() WriteRuntime(true) end)
  if InfCore and InfCore.Log then
    InfCore.Log("KeyboardSpeedControl: level reset to top")
  end
end

-- ---------------------------------------------------------------------------
-- Menu page
-- ---------------------------------------------------------------------------

function this.GenerateMenus()
  if not InfMenuDefs then return end

  InfMenuDefs.kscMenu = {
    description = "Keyboard Speed Control",
    options = {
      "Ivars.ksc_enabled",
      "Ivars.ksc_wheelStep",
      "Ivars.ksc_smooth",
      "Ivars.ksc_invert",
      "KeyboardSpeedControl_Core.ResetLevel",
    },
  }

  local function addToRoot(root)
    if not (root and root.options) then return end
    for _, ref in ipairs(root.options) do
      if ref == "InfMenuDefs.kscMenu" then return end
    end
    -- Append near the bottom, but above the trailing help entry.
    local index = #root.options + 1
    for i = #root.options, 1, -1 do
      if root.options[i] == "InfMenuCommands.GeneralHelpItem" then
        index = i
        break
      end
    end
    table.insert(root.options, index, "InfMenuDefs.kscMenu")
  end

  addToRoot(InfMenuDefs.inMissionMenu)
  addToRoot(InfMenuDefs.safeSpaceMenu)
end

function this.PostAllModulesLoad()
  -- Publish the persisted settings; level is intentionally omitted so the wheel
  -- remains the authority over the current speed.
  WriteRuntime(false)
end

_G.KeyboardSpeedControl_Core = this

return this
