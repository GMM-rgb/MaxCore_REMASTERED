os.native_execute("max-core-lua -v", { detached = false, wait = true })
local <const> StorageService = EngineCore:LoadService("StorageService")
local <const> WindowService = EngineCore:LoadService("WindowService")
if not StorageService._debug then StorageService:SetDebug(true) end

local DirectoryFiles = StorageService:ListDirectory(StorageService:GetBaseDirectory())
local PlannerData = StorageService:CreateFile({ path = "./planner_data.config" })
