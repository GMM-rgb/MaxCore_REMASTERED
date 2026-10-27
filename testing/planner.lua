os.native_execute("max-core-lua -v", { detached = false, wait = true })
local <const> RuntimeService = EngineCore:LoadService("RunnerService")
local <const> StorageService = EngineCore:LoadService("StorageService")
local <const> WindowService = EngineCore:LoadService("WindowService")
local <const> InputService = EngineCore:LoadService("InputService")
if not StorageService._debug then StorageService:SetDebug(true) end

local DirectoryFiles = StorageService:ListDirectory(StorageService:GetBaseDirectory())
local PlannerData = StorageService:CreateFile({ path = "./planner_data.config" })
local ToolWindow = WindowService:CreateWindow("Planning Tool", 800, 800) or nil
if not ToolWindow or EngineCore.typeof(ToolWindow) ~= "WindowObject" then return end
local InputListenerConnection = nil --- @type RunnerConnection?

---@return table<string>
local function ListenInput()
    local lines = {} --- @type table<string>

    local function get_active_keys()
        for _, key in ipairs(InputService.InputKeys) do
            if InputService:IsKeyPressed(key) then
                
            end
        end
    end

    InputListenerConnection = RuntimeService:Spawn("RenderStepped", function(wait, join)
        while true and type(wait) == "function" do local dt = wait(); get_active_keys() end
    end, { whenRunning = "replace" })

    return lines
end

InputService:BindAction("test", "f1", function(name, state, key)
    
end)

RuntimeService.Heartbeat:Connect(function(dt, wait, join)
    InputService:UpdateAll(); dt = wait()
end, { yieldable = true, whenRunning = "skip" })

RuntimeService:KeepAlive()
