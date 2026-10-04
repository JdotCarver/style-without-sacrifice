-- Style Without Sacrifice - Your Transmogrification Wardrobe. MIT.
local Settings=require("Settings")
local source=debug.getinfo(1,"S").source:gsub("^@","")
local root=assert(source:match("^(.*[/\\])Scripts[/\\][^/\\]+$"),"Cannot locate mod directory")
if type(_WCConfigure)~="function" or type(_WCStart)~="function" then
    print("[WardrobeTransmog] Native component unavailable; enable the complete mod and restart.\n")
    return
end
local values=Settings.load(root.."settings.ini")
local function apply(committed)
    values=Settings.normalize(committed)
    _WCConfigure(values.enabled,values.openKey,values.debugLogging)
end
-- Bind each supported key once. Apply only switches the selected binding.
for index,key in ipairs({Key.END,Key.F7,Key.F8,Key.F9}) do
    RegisterKeyBind(key,function()
        if values.enabled==1 and values.openKey==index-1 then _WCOpen() end
    end)
end
ExecuteInGameThread(function() apply(values);_WCStart(root) end)
local ok,err=pcall(function()
    require("dmm_api").subscribe("WardrobeTransmog",function(committed)
        apply(committed)
        if values.debugLogging==1 then print("[WardrobeTransmog] Settings applied.\n") end
    end)
end)
if not ok then print("[WardrobeTransmog] Settings subscription unavailable: "..tostring(err).."\n") end
