-- MGSVSpeed_Core.lua
-- Infinite Heaven external module: adds the MGSVSpeed knobs to the IH menu (F3)
-- and publishes them to plugins/MGSVSpeed.runtime, which MGSVSpeed.dll polls.
--
-- Install: copy to MGS_TPP/mod/modules/MGSVSpeed_Core.lua
-- IH auto-loads every .lua in that folder, so no other setup is needed.

local this = {}

if rawget(_G, "MGSVSpeed_Core") then
  return _G.MGSVSpeed_Core
end

local runtimeName = "MGSVSpeed.runtime"

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

local function WriteRuntime()
  local f = io.open(RuntimePath(), "w")
  if not f then return end
  f:write("enabled=" .. tostring(IvarValue("mgsvSpeed_enabled", 1)) .. "\n")
  f:write("level=" .. tostring(IvarValue("mgsvSpeed_level", 8)) .. "\n")
  f:write(string.format("smooth=%.3f\n", IvarValue("mgsvSpeed_smooth", 0.12)))
  f:write("invert=" .. tostring(IvarValue("mgsvSpeed_invert", 0)) .. "\n")
  f:close()
end

-- NOTE: no per-frame Update here. Calling io.open from the game's Lua update
-- deadlocks on the asset/file-system lock during loading (the game hangs at the
-- first Lua update). Writes only happen from menu callbacks, which are safe.

-- ---------------------------------------------------------------------------
-- Knobs
-- ---------------------------------------------------------------------------

local function OnChange()
  pcall(WriteRuntime)
end

this.registerIvars = {
  "mgsvSpeed_enabled",
  "mgsvSpeed_level",
  "mgsvSpeed_smooth",
  "mgsvSpeed_invert",
}

this.mgsvSpeed_enabled = {
  description = "Speed system",
  inMission = true,
  save = IvarProc.CATEGORY_EXTERNAL,
  range = Ivars.switchRange,
  settingNames = "set_switch",
  default = 1,
  OnChange = OnChange,
}

-- Approximate stand-stance speed for the default level table in MGSVSpeed.ini.
local levelSpeeds = { 1.41, 1.65, 1.90, 2.14, 3.21, 3.72, 4.23, 4.70 }

this.mgsvSpeed_level = {
  description = "Speed level",
  inMission = true,
  save = IvarProc.CATEGORY_EXTERNAL,
  default = 8,
  range = { min = 1, max = 8, increment = 1 },
  OnChange = OnChange,
  GetSettingText = function(self, setting)
    return string.format("%d/8  ~%.2f m/s", setting, levelSpeeds[setting] or 0)
  end,
}

this.mgsvSpeed_smooth = {
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

this.mgsvSpeed_invert = {
  description = "Invert wheel",
  inMission = true,
  save = IvarProc.CATEGORY_EXTERNAL,
  range = Ivars.switchRange,
  settingNames = "set_switch",
  default = 0,
  OnChange = OnChange,
}

-- ---------------------------------------------------------------------------
-- Menu page
-- ---------------------------------------------------------------------------

function this.GenerateMenus()
  if not InfMenuDefs then return end

  InfMenuDefs.mgsvSpeedMenu = {
    description = "MGSV Speed",
    options = {
      "Ivars.mgsvSpeed_enabled",
      "Ivars.mgsvSpeed_level",
      "Ivars.mgsvSpeed_smooth",
      "Ivars.mgsvSpeed_invert",
    },
  }

  local function addToRoot(root)
    if not (root and root.options) then return end
    for _, ref in ipairs(root.options) do
      if ref == "InfMenuDefs.mgsvSpeedMenu" then return end
    end
    table.insert(root.options, 1, "InfMenuDefs.mgsvSpeedMenu")
  end

  addToRoot(InfMenuDefs.inMissionMenu)
  addToRoot(InfMenuDefs.safeSpaceMenu)
end

function this.PostAllModulesLoad()
  -- Publish the persisted values so the DLL starts from the menu state.
  WriteRuntime()
end

_G.MGSVSpeed_Core = this

return this
