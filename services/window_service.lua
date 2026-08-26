-- =========================================================================
-- services/window_service.lua
-- =========================================================================
-- #service
local InstanceTyping = require("instance_type")

package.path = package.path
    .. ";./services/?.lua"
    .. ";./services/?/init.lua"
    .. ";./?.lua"
    .. ";./?/init.lua"

package.cpath = package.cpath 
    .. ";./build/?.dylib" 
    .. ";./build/Release/?.dylib" 
    .. ";./build/Debug/?.dylib" 
    .. ";./build/bin/?.dylib" 
    .. ";./build/?.so" 
    .. ";./build/?.dll"

local Objects = require("sub_modules.game_object")

---@class NativeWindowInterface
---@field create_window fun(title: string, width: integer, height: integer): integer
---@field get_dimensions fun(id: integer): integer, integer
---@field set_dimensions fun(id: integer, width: integer, height: integer): nil
---@field set_fullscreen fun(id: integer, enable: boolean): nil
---@field poll_events fun(): nil
---@field should_close fun(id: integer): boolean
---@field clear_canvas fun(id: integer, r: integer, g: integer, b: integer): nil
---@field draw_rect fun(id: integer, x: integer, y: integer, w: integer, h: integer, r: integer, g: integer, b: integer): nil
---@field draw_line fun(id: integer, x0: integer, y0: integer, x1: integer, y1: integer, r: integer, g: integer, b: integer): nil
---@field draw_circle fun(id: integer, cx: integer, cy: integer, r: integer, fill: boolean, red: integer, green: integer, blue: integer): nil
---@field create_image fun(width: integer, height: integer, pixelArray: table<integer, integer>?): integer
---@field draw_image fun(id: integer, imgId: integer, x: integer, y: integer): nil
---@field draw_cube fun(id: integer, px: number, py: number, pz: number, rx: number, ry: number, rz: number, sx: number, sy: number, sz: number, r: integer, g: integer, b: integer, wireframe: boolean): nil
---@field swap_buffers fun(id: integer): nil
---@field destroy fun(id: integer): nil

---@type boolean, NativeWindowInterface|string
local native_ok, window_interface = pcall(require, "window_management")

if not native_ok then
    print("\27[33m[WindowService Warning]\27[0m Failed to load window_interface: " .. tostring(window_interface))
end

---@class WindowObject
---@field private _id integer
---@field private _title string
---@field private _width integer
---@field private _height integer
---@field private _isOpen boolean
---@field private _objects table<integer, GameObject>
local WindowObject = {}
WindowObject.__index = WindowObject
InstanceTyping.SetType(WindowObject, "WindowObject")

---@class WindowService
---@field private _windows table<integer, WindowObject>
---@field private _activeWindow WindowObject?
local WindowService = {}
WindowService.__index = WindowService
InstanceTyping.SetType(WindowService, "WindowService")

-- =========================================================================
-- WINDOW OBJECT IMPLEMENTATION
-- =========================================================================

---Creates a new native window instance.
---@param title string? Title text displayed on window titlebar
---@param width integer? Framebuffer width in pixels
---@param height integer? Framebuffer height in pixels
---@return WindowObject?
function WindowObject.new(title, width, height)
    if not native_ok or type(window_interface) == "string" then return nil end

    local win_id = window_interface.create_window(title or "Native Viewport", width or 800, height or 600)
    if not win_id then return nil end

    ---@type WindowObject
    local self = setmetatable({}, WindowObject)
    self._id = win_id
    self._title = title or "Native Viewport"
    self._width = width or 800
    self._height = height or 600
    self._isOpen = true
    self._objects = {}

    return self
end

---Gets the current window canvas width and height.
---@return integer width Canvas pixel width
---@return integer height Canvas pixel height
function WindowObject:GetDimensions()
    if native_ok and type(window_interface) ~= "string" then
        local w, h = window_interface.get_dimensions(self._id)
        if w and h then
            self._width, self._height = w, h
            return w, h
        end
    end
    return self._width, self._height
end

---Sets new window dimensions.
---@param width integer New canvas width
---@param height integer New canvas height
---@return nil
function WindowObject:SetDimensions(width, height)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.set_dimensions(self._id, width, height)
        self._width = width
        self._height = height
    end
end

---Toggles borderless fullscreen mode.
---@param enable boolean Fullscreen enable status
---@return nil
function WindowObject:SetFullscreen(enable)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.set_fullscreen(self._id, enable)
        self:GetDimensions()
    end
end

---Polls events and checks whether the window remains active.
---@return boolean isRunning
function WindowObject:IsRunning()
    if not self._isOpen or not native_ok or type(window_interface) == "string" then return false end
    window_interface.poll_events()
    if window_interface.should_close(self._id) then
        self:Close()
        return false
    end
    return true
end

---Clears the window buffer with a background color.
---@param r integer? Red component (0-255)
---@param g integer? Green component (0-255)
---@param b integer? Blue component (0-255)
---@return nil
function WindowObject:ClearCanvas(r, g, b)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.clear_canvas(self._id, r or 24, g or 24, b or 24)
    end
end

---Draws raw 2D rectangle primitive directly to buffer.
---@param x number X coordinate
---@param y number Y coordinate
---@param w number Width
---@param h number Height
---@param r integer? Red component (0-255)
---@param g integer? Green component (0-255)
---@param b integer? Blue component (0-255)
---@return nil
function WindowObject:DrawRectangle(x, y, w, h, r, g, b)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.draw_rect(self._id, math.floor(x), math.floor(y), math.floor(w), math.floor(h), r or 255, g or 255, b or 255)
    end
end

---Draws raw 2D line primitive directly to buffer.
---@param x0 number Start X coordinate
---@param y0 number Start Y coordinate
---@param x1 number End X coordinate
---@param y1 number End Y coordinate
---@param r integer? Red component (0-255)
---@param g integer? Green component (0-255)
---@param b integer? Blue component (0-255)
---@return nil
function WindowObject:DrawLine(x0, y0, x1, y1, r, g, b)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.draw_line(self._id, math.floor(x0), math.floor(y0), math.floor(x1), math.floor(y1), r or 255, g or 255, b or 255)
    end
end

---Draws raw 2D circle primitive directly to buffer.
---@param cx number Center X coordinate
---@param cy number Center Y coordinate
---@param radius number Circle radius
---@param fill boolean? Fill shape toggle
---@param r integer? Red component (0-255)
---@param g integer? Green component (0-255)
---@param b integer? Blue component (0-255)
---@return nil
function WindowObject:DrawCircle(cx, cy, radius, fill, r, g, b)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.draw_circle(self._id, math.floor(cx), math.floor(cy), math.floor(radius), fill or false, r or 255, g or 255, b or 255)
    end
end

---Creates a software image buffer from raw pixel data.
---@param width integer Image width
---@param height integer Image height
---@param pixelArray table<integer, integer>? Pixel color buffer table
---@return integer? imageId Handle identifier
function WindowObject:CreateImage(width, height, pixelArray)
    if native_ok and type(window_interface) ~= "string" then
        return window_interface.create_image(width, height, pixelArray)
    end
    return nil
end

---Draws raw image primitive directly to buffer.
---@param imageId integer Image handle ID
---@param x number X coordinate
---@param y number Y coordinate
---@return nil
function WindowObject:DrawImage(imageId, x, y)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.draw_image(self._id, imageId, math.floor(x), math.floor(y))
    end
end

---Draws raw 3D cube primitive directly to buffer.
---@param px number? Position X
---@param py number? Position Y
---@param pz number? Position Z
---@param rx number? Rotation X (radians)
---@param ry number? Rotation Y (radians)
---@param rz number? Rotation Z (radians)
---@param sx number? Scale X
---@param sy number? Scale Y
---@param sz number? Scale Z
---@param r integer? Red component (0-255)
---@param g integer? Green component (0-255)
---@param b integer? Blue component (0-255)
---@param wireframe boolean? Render wireframe if true
---@return nil
function WindowObject:DrawCube(px, py, pz, rx, ry, rz, sx, sy, sz, r, g, b, wireframe)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.draw_cube(
            self._id,
            px or 0.0, py or 0.0, pz or 3.0,
            rx or 0.0, ry or 0.0, rz or 0.0,
            sx or 1.0, sy or 1.0, sz or 1.0,
            r or 0, g or 255, b or 0,
            wireframe or false
        )
    end
end

-- -------------------------------------------------------------------------
-- GAME OBJECT CREATION & MANAGEMENT METHODS
-- -------------------------------------------------------------------------

---Instantiates a modifiable RectObject extending GameObject.
---@param x number? Position X
---@param y number? Position Y
---@param w number? Width
---@param h number? Height
---@param r integer? Red component (0-255)
---@param g integer? Green component (0-255)
---@param b integer? Blue component (0-255)
---@return RectObject
function WindowObject:CreateRect(x, y, w, h, r, g, b)
    local obj = Objects.RectObject.new(x, y, w, h, r, g, b)
    table.insert(self._objects, obj)
    return obj
end

---Instantiates a modifiable CircleObject extending GameObject.
---@param cx number? Center X coordinate
---@param cy number? Center Y coordinate
---@param radius number? Radius size
---@param fill boolean? Fill shape toggle
---@param r integer? Red component (0-255)
---@param g integer? Green component (0-255)
---@param b integer? Blue component (0-255)
---@return CircleObject
function WindowObject:CreateCircle(cx, cy, radius, fill, r, g, b)
    local obj = Objects.CircleObject.new(cx, cy, radius, fill, r, g, b)
    table.insert(self._objects, obj)
    return obj
end

---Instantiates a modifiable LineObject extending GameObject.
---@param x0 number? Start X coordinate
---@param y0 number? Start Y coordinate
---@param x1 number? End X coordinate
---@param y1 number? End Y coordinate
---@param r integer? Red component (0-255)
---@param g integer? Green component (0-255)
---@param b integer? Blue component (0-255)
---@return LineObject
function WindowObject:CreateLine(x0, y0, x1, y1, r, g, b)
    local obj = Objects.LineObject.new(x0, y0, x1, y1, r, g, b)
    table.insert(self._objects, obj)
    return obj
end

---Instantiates a modifiable ImageObject extending GameObject.
---@param imageId integer Image resource handle ID
---@param x number? Position X coordinate
---@param y number? Position Y coordinate
---@return ImageObject
function WindowObject:CreateImageObject(imageId, x, y)
    ---@type ImageObject
    local obj = Objects.ImageObject.new(imageId, x, y)
    table.insert(self._objects, obj)
    return obj
end

---Instantiates a modifiable CubeObject extending GameObject.
---@param px number? Position X coordinate
---@param py number? Position Y coordinate
---@param pz number? Position Z coordinate
---@param size number? Uniform scale multiplier
---@param r integer? Red component (0-255)
---@param g integer? Green component (0-255)
---@param b integer? Blue component (0-255)
---@return CubeObject
function WindowObject:CreateCube(px, py, pz, size, r, g, b)
    local obj = Objects.CubeObject.new(px, py, pz, size, r, g, b)
    table.insert(self._objects, obj)
    return obj
end

---Renders an individual GameObject instance.
---@param gameObject GameObject Target object implementing a Render method
---@return nil
function WindowObject:RenderObject(gameObject)
    if gameObject and type(gameObject.Render) == "function" then
        gameObject:Render(self)
    end
end

---Renders all tracked GameObjects bound to this window context.
---@return nil
function WindowObject:RenderAll()
    for _, obj in ipairs(self._objects) do
        self:RenderObject(obj)
    end
end

---Swaps the native window framebuffers.
---@return nil
function WindowObject:SwapBuffers()
    if native_ok and type(window_interface) ~= "string" then
        window_interface.swap_buffers(self._id)
    end
end

---Returns the window native handle identifier.
---@return integer id
function WindowObject:GetId()
    return self._id
end

---Closes the window and releases native resources.
---@return nil
function WindowObject:Close()
    if self._isOpen and native_ok and type(window_interface) ~= "string" then
        window_interface.destroy(self._id)
        self._isOpen = false
        self._objects = {}
    end
end

-- =========================================================================
-- MAIN WINDOW SERVICE IMPLEMENTATION
-- =========================================================================

---Constructs the main WindowService instance.
---@return WindowService
function WindowService.new()
    ---@type WindowService
    local self = setmetatable({}, WindowService)
    self._windows = {}
    self._activeWindow = nil
    return self
end

---Creates and tracks a new managed window object.
---@param title string? Window title
---@param width integer? Framebuffer width
---@param height integer? Framebuffer height
---@return WindowObject?
function WindowService:CreateWindow(title, width, height)
    local win = WindowObject.new(title, width, height)
    if win ~= nil then
        self._windows[win:GetId()] = win
        if not self._activeWindow then
            self._activeWindow = win
        end
    end
    return win
end

---Gets the primary focused or target active window handle.
---@param id integer? Target window identifier handle
---@return WindowObject?
function WindowService:GetWindow(id)
    if id then
        return self._windows[id]
    end
    return self._activeWindow
end

---Closes all active managed windows.
---@return nil
function WindowService:CloseAll()
    for id, win in pairs(self._windows) do
        win:Close()
        self._windows[id] = nil
    end self._activeWindow = nil
end

return WindowService
