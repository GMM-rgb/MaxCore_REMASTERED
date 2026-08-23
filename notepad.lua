local core <const> = require("max_core").call()
local storage = core:LoadService("StorageService")
local contents = storage:GetFile("./notes.txt"):Read()
io.stdout:write(contents .. "\n")

---@type string
local input = io.read()

storage:CreateFile({
    path = "./notes.txt", debug = true,
}):Write(tostring(input))
