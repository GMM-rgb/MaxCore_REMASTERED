os.native_execute("max-core-lua -v", { detached = false, wait = true })
local <const> StorageService = EngineCore:LoadService("StorageService")
if not StorageService._debug then StorageService:SetDebug(true) end


