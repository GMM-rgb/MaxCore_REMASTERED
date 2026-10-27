local <const> RuntimeService = EngineCore:LoadService("RunnerService")
local <const> StorageService = EngineCore:LoadService("StorageService")
local <const> WindowService = EngineCore:LoadService("WindowService")
local <const> InputService = EngineCore:LoadService("InputService")
if not StorageService._debug then StorageService:SetDebug(true) end

local DirectoryFiles = StorageService:ListDirectory(StorageService:GetBaseDirectory())
local PlannerData = StorageService:CreateFile({ path = "./planner_data.config" })
local ToolWindow = WindowService:CreateWindow("Planning Tool", 800, 800) or nil
if not ToolWindow or EngineCore.typeof(ToolWindow) ~= "WindowObject" then return end
local WindowFocused = EngineCore.Event.new("window-focus-change")
local InputChanged = EngineCore.Event.new("input-changed-value")
local InputListenerConnection = nil --- @type RunnerConnection?
local ListenerRunning = false
local ComposedInput = ""

local BlacklistedKeys = {
    "return", "enter",
};

local special_keys = {
    ["tab"] = "\t",
    ["return"] = "\n",
    ["enter"] = "\n",
};


---@param state InputActionState
local function ClearInputListener(_, state, _)
    if not state or state == "Released" then return end
    if ComposedInput:len() > 0 then ComposedInput = "" end
    print "CLEARED INPUT VALUE"
end

local function InitalizeInputListener()
    for _, key in ipairs(InputService.InputKeys) do
        if type(BlacklistedKeys) == "table" and not BlacklistedKeys[key] then
            InputService:BindAction("input-listner-" .. key, key, function(actionName, state, keyName)
                if type(state) ~= "nil" and state == "Pressed" then InputChanged:Fire(keyName) end
            end)
        end
    end
end

-- InputListenerConnection = RuntimeService:Spawn("RenderStepped", function(wait, join)
--     while true and type(wait) == "function" do local dt = wait(); compose_message() end
-- end, { whenRunning = "skip" }); ListenerRunning = true

---@return nil
local function WindowRunning()
    if ToolWindow and not ToolWindow:IsRunning() then
        RuntimeService._native:shutdown()
        print("SHUTTING DOWN...")
        os.exit(true, true)
    end
end

RuntimeService.RenderStepped:Connect(function(dt)
    ToolWindow:SwapBuffers()
    xpcall(WindowRunning, error)
end, { whenRunning = "replace", safe = true, priority = 0 })

RuntimeService.Heartbeat:Connect(function(dt, wait, join)
    InputService:UpdateAll(); dt = wait()
    InputService:SetGlobalInput(ToolWindow:IsFocused())
end, { yieldable = true, whenRunning = "skip" })

---@param value string
InputChanged:Connect(function(value, ...)
    ToolWindow:ClearCanvas(); ToolWindow:SwapBuffers()
    ToolWindow:DrawText(value, 0, 0, 2)
end)

InitalizeInputListener()
InputService:BindAction("clear", "enter", ClearInputListener)
RuntimeService:KeepAlive()
