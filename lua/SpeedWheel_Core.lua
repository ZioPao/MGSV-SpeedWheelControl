-- SpeedWheel_Core.lua
-- Infinite Heaven external module: adds the Speed Wheel Control knobs to the IH menu (F3)
-- and publishes them to plugins/SpeedWheel.runtime, which SpeedWheel.dll polls.
--
-- Install: copy to MGS_TPP/mod/modules/SpeedWheel_Core.lua
-- IH auto-loads every .lua in that folder, so no other setup is needed.

local this = { updateOutsideGame = true, updateRate = 0 }

if rawget(_G, "SpeedWheel_Core") then
  return _G.SpeedWheel_Core
end

local runtimeName = "SpeedWheel.runtime"
local writeSeq = 0
local idroidState = 0
local lastIdroid = nil

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
  writeSeq = writeSeq + 1
  f:write("seq=" .. tostring(writeSeq) .. "\n")
  f:write("enabled=" .. tostring(IvarValue("sw_enabled", 1)) .. "\n")
  f:write(string.format("smooth=%.3f\n", IvarValue("sw_smooth", 0.12)))
  f:write("invert=" .. tostring(IvarValue("sw_invert", 0)) .. "\n")
  f:write("step=" .. tostring(IvarValue("sw_wheelStep", 1)) .. "\n")
  f:write("idroid=" .. tostring(idroidState) .. "\n")
  f:close()
end

-- NOTE: no per-frame Update here. Calling io.open from the game's Lua update
-- deadlocks on the asset/file-system lock during loading (the game hangs at the
-- first Lua update). Writes only happen from menu callbacks, which are safe.
--
-- The one exception is the iDroid flag below: it is published only when the
-- state flips (open/close), after loading, so it is one write per toggle.

-- Infinite Heaven already computes the iDroid state every frame. Publish it on
-- change so the DLL can stop the wheel from changing speed while the map is
-- open (the map zoom does not read the pad wheel through any hookable path).
local function CurrentIdroid(currentChecks)
  if currentChecks and currentChecks.inIdroid ~= nil then
    return currentChecks.inIdroid and 1 or 0
  end
  if InfMain and InfMain.execChecks and InfMain.execChecks.inIdroid ~= nil then
    return InfMain.execChecks.inIdroid and 1 or 0
  end
  return nil
end

local reportedSource = false

local function InGame(currentChecks)
  if currentChecks and currentChecks.inGame ~= nil then return currentChecks.inGame end
  if InfMain and InfMain.execChecks and InfMain.execChecks.inGame ~= nil then
    return InfMain.execChecks.inGame
  end
  return true
end

function this.Update(currentChecks)
  if not InGame(currentChecks) then return end
  local idroid = CurrentIdroid(currentChecks)
  if not reportedSource and InfCore and InfCore.Log then
    reportedSource = true
    InfCore.Log("SpeedWheel: idroid state " ..
      (idroid ~= nil and "available" or "UNAVAILABLE") ..
      " (checks=" .. tostring(currentChecks ~= nil) .. ")", false, true)
  end
  if idroid == nil or idroid == lastIdroid then return end
  lastIdroid = idroid
  idroidState = idroid
  pcall(WriteRuntime)
end

-- ---------------------------------------------------------------------------
-- Knobs
-- ---------------------------------------------------------------------------

local function OnChange()
  pcall(WriteRuntime)
end

this.registerIvars = {
  "sw_enabled",
  "sw_wheelStep",
  "sw_smooth",
  "sw_invert",
}

this.sw_enabled = {
  description = "Speed system",
  inMission = true,
  save = IvarProc.CATEGORY_EXTERNAL,
  range = Ivars.switchRange,
  settingNames = "set_switch",
  default = 1,
  OnChange = OnChange,
}

this.sw_wheelStep = {
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

this.sw_smooth = {
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

this.sw_invert = {
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

  InfMenuDefs.swMenu = {
    description = "Speed Wheel Control",
    options = {
      "Ivars.sw_enabled",
      "Ivars.sw_wheelStep",
      "Ivars.sw_smooth",
      "Ivars.sw_invert",
    },
  }

  local function addToRoot(root)
    if not (root and root.options) then return end
    for _, ref in ipairs(root.options) do
      if ref == "InfMenuDefs.swMenu" then return end
    end
    -- Append near the bottom, but above the trailing help entry.
    local index = #root.options + 1
    for i = #root.options, 1, -1 do
      if root.options[i] == "InfMenuCommands.GeneralHelpItem" then
        index = i
        break
      end
    end
    table.insert(root.options, index, "InfMenuDefs.swMenu")
  end

  addToRoot(InfMenuDefs.inMissionMenu)
  addToRoot(InfMenuDefs.safeSpaceMenu)
end

function this.PostAllModulesLoad()
  -- Publish the persisted settings; the wheel remains the authority over speed.
  WriteRuntime()
end

_G.SpeedWheel_Core = this

return this
