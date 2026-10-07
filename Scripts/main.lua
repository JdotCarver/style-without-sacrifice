-- Style Without Sacrifice - Your Transmogrification Wardrobe. MIT.
local Settings=require("Settings")
local source=debug.getinfo(1,"S").source:gsub("^@","")
local root=assert(source:match("^(.*[/\\])Scripts[/\\][^/\\]+$"),"Cannot locate mod directory")
ModDiagnosticLevel=require("ModLogLevels").readLevel(root.."settings.ini")
local loaded,values=pcall(Settings.load,root.."settings.ini")
if not loaded then
    if ModDiagnosticLevel>=1 then print("[ERROR] Settings preparation failed: "..tostring(values).."\n") end
    return
end
ModDiagnosticLevel=values.logLevel
if type(_WCConfigureLogV2)~="function" or type(_WCStart)~="function" then
    if values.logLevel>=1 then print("[WardrobeTransmog][ERROR] Native component unavailable; enable the complete mod and restart.\n") end
    return
end
local function apply(committed)
    values=Settings.normalize(committed)
    ModDiagnosticLevel=values.logLevel
    _WCConfigureLogV2(values.enabled,values.openKey,values.logLevel)
end
-- Bind each supported key once. Apply only switches the selected binding.
for index,key in ipairs({Key.END,Key.F7,Key.F8,Key.F9}) do
    RegisterKeyBind(key,function()
        if values.enabled==1 and values.openKey==index-1 then _WCOpen() end
    end)
end
ExecuteInGameThread(function() apply(values);_WCStart(root) end)
local ok,err=pcall(function()
    require("ModDmmApi").subscribe("WardrobeTransmog",function(committed)
        local requested=Settings.normalize(committed)
        values=requested
        ModDiagnosticLevel=values.logLevel
        local applied,issue=pcall(apply,committed)
        if not applied and values.logLevel>=1 then print("[ERROR] Settings Apply failed: "..tostring(issue).."\n") end
        if values.logLevel>=3 then print("[WardrobeTransmog] Settings applied.\n") end
    end)
end)
if not ok and values.logLevel>=2 then print("[WardrobeTransmog] Settings subscription unavailable: "..tostring(err).."\n") end
