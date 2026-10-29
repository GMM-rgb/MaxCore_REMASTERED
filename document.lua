local StorageService = EngineCore:LoadService("StorageService")
local RuntimeService = EngineCore:LoadService("RunnerService")
local WindowService = EngineCore:LoadService("WindowService")
local SoundService = EngineCore:LoadService("SoundService")
local WindowDocument = WindowService:CreateWindow("Document")

SoundService:SetStorageService(StorageService); SoundService:SetCacheFolder("./app_audio/")
local MusicAudio = SoundService:LoadSound("https://music.youtube.com/watch?v=sFxsp2awVgg&si=OoQ3mGmUJ9jAyEfN&drli=1")
MusicAudio:SetVolume(1.5); MusicAudio:Play()

local ToolbarConfiguration = {
    ["container"] = {
        
    },
};

---@param brightness number?
---@return table
local function ConstructToolbar(brightness)
    for element, configuration in pairs(ToolbarConfiguration) do
        
    end

    return {}
end

RuntimeService.Heartbeat:Connect(function(dt, wait, join)
    if not WindowDocument or not WindowDocument:IsRunning() then return end
    dt = wait(); WindowDocument:SwapBuffers(); WindowDocument:StepPhysics(dt)
end, { yieldable = true, whenRunning = "allow" })

RuntimeService:KeepAlive()
