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
local PhysicsModule = require("sub_modules.physics_object")

---@alias Vertex3 { [1]: number, [2]: number, [3]: number }
---@alias MeshFace integer[] # 1-based vertex indices into a mesh's vertex list, wound counter-clockwise (viewed from outside) for correct-facing shading

---@alias AliasingMode
---| '"2d"' # 2D primitives: lines, circles, filled polygons
---| '"3d"' # 3D primitives: cube/mesh wireframe edges, points, and solid faces

---@class NativeWindowInterface
---@field create_window fun(title: string, width: integer, height: integer): integer
---@field get_dimensions fun(id: integer): integer, integer
---@field set_dimensions fun(id: integer, width: integer, height: integer): nil
---@field set_fullscreen fun(id: integer, enable: boolean): nil
---@field get_position fun(id: integer): integer, integer
---@field set_position fun(id: integer, x: integer, y: integer): nil
---@field get_display_resolution fun(): integer, integer
---@field poll_events fun(): nil
---@field should_close fun(id: integer): boolean
---@field clear_canvas fun(id: integer, r: integer, g: integer, b: integer): nil
---@field draw_rect fun(id: integer, x: integer, y: integer, w: integer, h: integer, r: integer, g: integer, b: integer): nil
---@field draw_line fun(id: integer, x0: integer, y0: integer, x1: integer, y1: integer, r: integer, g: integer, b: integer): nil
---@field draw_circle fun(id: integer, cx: integer, cy: integer, r: integer, fill: boolean, red: integer, green: integer, blue: integer): nil
---@field draw_text fun(id: integer, text: string, x: integer, y: integer, scale: integer?, r: integer?, g: integer?, b: integer?, quality: integer?): integer effectiveQuality
---@field get_max_text_quality fun(): integer
---@field set_aliasing_quality fun(id: integer, mode: string, quality: integer?): integer effectiveQuality
---@field get_aliasing_quality fun(id: integer, mode: string): integer
---@field get_max_alias_quality fun(): integer
---@field draw_polygon fun(id: integer, pointsOrFill: table|boolean, ...): nil
---@field create_image fun(width: integer, height: integer, pixelArray: table<integer, integer>?): integer
---@field draw_image fun(id: integer, imgId: integer, x: integer, y: integer): nil
---@field draw_cube fun(id: integer, px: number, py: number, pz: number, rx: number, ry: number, rz: number, sx: number, sy: number, sz: number, r: integer, g: integer, b: integer, fillMode: CubeFillMode|boolean|integer?): nil
---@field draw_mesh fun(id: integer, vertices: Vertex3[], faces: MeshFace[], px: number, py: number, pz: number, rx: number, ry: number, rz: number, sx: number, sy: number, sz: number, r: integer, g: integer, b: integer, fillMode: CubeFillMode|boolean|integer?): nil
---@field create_camera fun(px: number?, py: number?, pz: number?, fov: number?, nearPlane: number?, farPlane: number?): integer
---@field destroy_camera fun(camId: integer): nil
---@field camera_set_position fun(camId: integer, x: number, y: number, z: number): nil
---@field camera_get_position fun(camId: integer): number, number, number
---@field camera_set_rotation fun(camId: integer, pitch: number, yaw: number, roll: number?): nil
---@field camera_get_rotation fun(camId: integer): number, number, number
---@field camera_set_fov fun(camId: integer, fov: number): nil
---@field camera_get_fov fun(camId: integer): number
---@field camera_set_clip_planes fun(camId: integer, nearPlane: number, farPlane: number): nil
---@field set_active_camera fun(winId: integer, camId: integer): nil
---@field get_active_camera fun(winId: integer): integer?
---@field create_light fun(dx: number?, dy: number?, dz: number?, ambient: number?, intensity: number?): integer
---@field destroy_light fun(lightId: integer): nil
---@field light_set_direction fun(lightId: integer, dx: number, dy: number, dz: number): nil
---@field light_get_direction fun(lightId: integer): number, number, number
---@field light_set_ambient fun(lightId: integer, ambient: number): nil
---@field light_get_ambient fun(lightId: integer): number
---@field light_set_intensity fun(lightId: integer, intensity: number): nil
---@field light_get_intensity fun(lightId: integer): number
---@field set_active_light fun(winId: integer, lightId: integer): nil
---@field get_active_light fun(winId: integer): integer?
---@field swap_buffers fun(id: integer): nil
---@field destroy fun(id: integer): nil

---@type boolean, NativeWindowInterface|string
local native_ok, window_interface = pcall(require, "window_management")

if not native_ok then
    print("\27[33m[WindowService Warning]\27[0m Failed to load window_interface: " .. tostring(window_interface))
end

-- =========================================================================
-- CAMERA OBJECT IMPLEMENTATION
-- =========================================================================

---@class CameraObject
---@field private _id integer
local CameraObject = {}
CameraObject.__index = CameraObject
InstanceTyping.SetType(CameraObject, "CameraObject")

---Creates a new 3D camera used to position and orient a window's viewport.
---Not bound to any window by default -- pair it with WindowObject:SetActiveCamera.
---@param px number? Position X
---@param py number? Position Y
---@param pz number? Position Z
---@param fov number? Vertical field of view, in degrees (default 90)
---@param nearPlane number? Near clip plane (default 0.1)
---@param farPlane number? Far clip plane (default 100)
---@return CameraObject?
function CameraObject.new(px, py, pz, fov, nearPlane, farPlane)
    if not native_ok or type(window_interface) == "string" then return nil end

    local camId = window_interface.create_camera(px or 0.0, py or 0.0, pz or 0.0, fov or 90.0, nearPlane or 0.1, farPlane or 100.0)
    if not camId then return nil end

    ---@type CameraObject
    local self = setmetatable({}, CameraObject)
    self._id = camId
    return self
end

---Sets the camera's world position.
---@param x number
---@param y number
---@param z number
---@return nil
function CameraObject:SetPosition(x, y, z)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.camera_set_position(self._id, x, y, z)
    end
end

---Gets the camera's world position.
---@return number x
---@return number y
---@return number z
function CameraObject:GetPosition()
    if native_ok and type(window_interface) ~= "string" then
        return window_interface.camera_get_position(self._id)
    end
    return 0, 0, 0
end

---Sets the camera's pitch/yaw/roll orientation, in radians.
---@param pitch number Rotation around the X axis (look up/down)
---@param yaw number Rotation around the Y axis (look left/right)
---@param roll number? Rotation around the Z axis (reserved, default 0)
---@return nil
function CameraObject:SetRotation(pitch, yaw, roll)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.camera_set_rotation(self._id, pitch, yaw, roll or 0.0)
    end
end

---Gets the camera's current pitch/yaw/roll orientation, in radians.
---@return number pitch
---@return number yaw
---@return number roll
function CameraObject:GetRotation()
    if native_ok and type(window_interface) ~= "string" then
        return window_interface.camera_get_rotation(self._id)
    end
    return 0, 0, 0
end

---Sets the camera's vertical field of view, in degrees.
---@param fov number
---@return nil
function CameraObject:SetFOV(fov)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.camera_set_fov(self._id, fov)
    end
end

---Gets the camera's vertical field of view, in degrees.
---@return number fov
function CameraObject:GetFOV()
    if native_ok and type(window_interface) ~= "string" then
        return window_interface.camera_get_fov(self._id)
    end
    return 90.0
end

---Sets the camera's near/far clip planes.
---@param nearPlane number
---@param farPlane number
---@return nil
function CameraObject:SetClipPlanes(nearPlane, farPlane)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.camera_set_clip_planes(self._id, nearPlane, farPlane)
    end
end

---Returns the camera's native handle identifier.
---@return integer id
function CameraObject:GetId()
    return self._id
end

---Destroys the camera and releases its native resources. Any window
---still pointing at this camera automatically falls back to the
---default view (origin, no rotation, 90 degree FOV).
---@return nil
function CameraObject:Destroy()
    if native_ok and type(window_interface) ~= "string" then
        window_interface.destroy_camera(self._id)
    end
end

-- =========================================================================
-- LIGHT OBJECT IMPLEMENTATION ("BASIC SHADERS")
-- =========================================================================

---@class LightObject
---@field private _id integer
local LightObject = {}
LightObject.__index = LightObject
InstanceTyping.SetType(LightObject, "LightObject")

---Creates a directional light used for basic per-face (flat/Lambertian)
---shading on solid-filled cubes/meshes. Not bound to any window by
---default -- pair it with WindowObject:SetActiveLight.
---@param dx number? Light travel direction X (default 0.4)
---@param dy number? Light travel direction Y (default -0.7, i.e. shining down)
---@param dz number? Light travel direction Z (default 0.6)
---@param ambient number? Base brightness on faces facing away from the light, 0-1 (default 0.35)
---@param intensity number? Diffuse contribution multiplier (default 1.0)
---@return LightObject?
function LightObject.new(dx, dy, dz, ambient, intensity)
    if not native_ok or type(window_interface) == "string" then return nil end

    local lightId = window_interface.create_light(dx or 0.4, dy or -0.7, dz or 0.6, ambient or 0.35, intensity or 1.0)
    if not lightId then return nil end

    ---@type LightObject
    local self = setmetatable({}, LightObject)
    self._id = lightId
    return self
end

---Sets the direction the light travels (from source toward the scene).
---@param dx number
---@param dy number
---@param dz number
---@return nil
function LightObject:SetDirection(dx, dy, dz)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.light_set_direction(self._id, dx, dy, dz)
    end
end

---Gets the light's current travel direction.
---@return number dx
---@return number dy
---@return number dz
function LightObject:GetDirection()
    if native_ok and type(window_interface) ~= "string" then
        return window_interface.light_get_direction(self._id)
    end
    return 0.4, -0.7, 0.6
end

---Sets the ambient (base) brightness applied even on faces facing away
---from the light, 0-1.
---@param ambient number
---@return nil
function LightObject:SetAmbient(ambient)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.light_set_ambient(self._id, ambient)
    end
end

---Gets the light's ambient brightness.
---@return number ambient
function LightObject:GetAmbient()
    if native_ok and type(window_interface) ~= "string" then
        return window_interface.light_get_ambient(self._id)
    end
    return 0.35
end

---Sets the diffuse contribution multiplier.
---@param intensity number
---@return nil
function LightObject:SetIntensity(intensity)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.light_set_intensity(self._id, intensity)
    end
end

---Gets the light's diffuse intensity.
---@return number intensity
function LightObject:GetIntensity()
    if native_ok and type(window_interface) ~= "string" then
        return window_interface.light_get_intensity(self._id)
    end
    return 1.0
end

---Returns the light's native handle identifier.
---@return integer id
function LightObject:GetId()
    return self._id
end

---Destroys the light and releases its native resources. Any window
---still pointing at this light automatically falls back to unlit
---(flat input color) rendering.
---@return nil
function LightObject:Destroy()
    if native_ok and type(window_interface) ~= "string" then
        window_interface.destroy_light(self._id)
    end
end

-- =========================================================================
-- WINDOW OBJECT IMPLEMENTATION
-- =========================================================================

---@class WindowObject
---@field private _id integer
---@field private _title string
---@field private _width integer
---@field private _height integer
---@field private _isOpen boolean
---@field private _objects table<integer, GameObject>
---@field private _activeCamera CameraObject?
---@field private _activeLight LightObject?
---@field private _physicsWorld PhysicsWorld?
---@field private _physicsLinks table[] # {gameObject=GameObject, body=PhysicsBody} pairs synced every StepPhysics
local WindowObject = {}
WindowObject.__index = WindowObject
InstanceTyping.SetType(WindowObject, "WindowObject")

---@class WindowService
---@field private _windows table<integer, WindowObject>
---@field private _activeWindow WindowObject?
local WindowService = {}
WindowService.__index = WindowService
InstanceTyping.SetType(WindowService, "WindowService")

-- Common collision-group bit flags, re-exported for convenience so callers
-- don't need to separately require("sub_modules.physics_object") just to
-- get e.g. WindowService.PhysicsGroups.Enemy.
---@type PhysicsGroups
WindowService.PhysicsGroups = PhysicsModule.PhysicsGroups

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
    self._activeCamera = nil
    self._activeLight = nil
    self._physicsWorld = nil
    self._physicsLinks = {}

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

---Gets the window's current screen position (top-left corner on
---Windows/X11; raw Cocoa frame origin, bottom-left based, on macOS).
---@return integer x
---@return integer y
function WindowObject:GetPosition()
    if native_ok and type(window_interface) ~= "string" then
        local x, y = window_interface.get_position(self._id)
        return x or 0, y or 0
    end
    return 0, 0
end

---Moves the window to a new screen position.
---@param x integer
---@param y integer
---@return nil
function WindowObject:SetPosition(x, y)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.set_position(self._id, math.floor(x), math.floor(y))
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

---Draws ASCII text string supporting \n linebreaks and \t tabstops.
---`quality` is an integer sampling level: 0/nil (default) renders exactly
---like before -- hard nearest-neighbor 8x8-per-glyph blockiness. 1 and up
---bilinear-supersamples the glyph for smoother edges at the same `scale`;
---higher numbers sample more and look a bit smoother, up to whatever
---WindowService.GetMaxTextQuality() reports -- ask for more than that and
---it silently clamps down to the max instead of failing. Returns the
---quality level actually used so you can tell when a request got capped.
---@param text string String text content
---@param x number Start X coordinate
---@param y number Start Y coordinate
---@param scale integer? Text scaling factor
---@param r integer? Red component (0-255)
---@param g integer? Green component (0-255)
---@param b integer? Blue component (0-255)
---@param quality integer? Sampling/anti-aliasing level (default 0 = legacy blocky rendering)
---@return integer effectiveQuality The quality level actually used, after clamping
function WindowObject:DrawText(text, x, y, scale, r, g, b, quality)
    if native_ok and type(window_interface) ~= "string" then
        return window_interface.draw_text(self._id, tostring(text or ""), math.floor(x), math.floor(y), scale or 1, r or 255, g or 255, b or 255, quality or 0)
    end
    return 0
end

---Draws polygon primitive using a table or unpacked vararg vertex coordinates.
---@param pointsOrFill table|boolean Table of vertices or fill boolean if unpacked
---@param ... any Color values or coordinate parameters
---@return nil
function WindowObject:DrawPolygon(pointsOrFill, ...)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.draw_polygon(self._id, pointsOrFill, ...)
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

---Draws raw 3D cube primitive directly to buffer, rendered through this
---window's active camera (see CreateCamera / SetActiveCamera) and shaded
---by its active light if one is bound (see CreateLight / SetActiveLight).
---With no camera/light attached, behaves exactly as before either feature
---existed: fixed camera at the origin, flat unlit color.
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
---@param fillMode CubeFillMode|boolean|integer? "wireframe"|"solid"|"point" (default "solid"). Legacy boolean still works: true = wireframe.
---@return nil
function WindowObject:DrawCube(px, py, pz, rx, ry, rz, sx, sy, sz, r, g, b, fillMode)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.draw_cube(
            self._id,
            px or 0.0, py or 0.0, pz or 3.0,
            rx or 0.0, ry or 0.0, rz or 0.0,
            sx or 1.0, sy or 1.0, sz or 1.0,
            r or 0, g or 255, b or 0,
            fillMode == nil and "solid" or fillMode
        )
    end
end

---Draws a custom 3D mesh -- the 3D equivalent of DrawPolygon. Runs your
---own vertex/face list through the exact same camera + projection +
---lighting pipeline as DrawCube.
---@param vertices Vertex3[] Local-space vertex list, e.g. {{0,0,0}, {1,0,0}, ...}
---@param faces MeshFace[] Each face is a list of 1-based vertex indices (3+), wound counter-clockwise viewed from outside for correct shading
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
---@param fillMode CubeFillMode|boolean|integer? "wireframe"|"solid"|"point" (default "solid")
---@return nil
function WindowObject:DrawMesh(vertices, faces, px, py, pz, rx, ry, rz, sx, sy, sz, r, g, b, fillMode)
    if native_ok and type(window_interface) ~= "string" then
        window_interface.draw_mesh(
            self._id,
            vertices or {}, faces or {},
            px or 0.0, py or 0.0, pz or 3.0,
            rx or 0.0, ry or 0.0, rz or 0.0,
            sx or 1.0, sy or 1.0, sz or 1.0,
            r or 255, g or 255, b or 255,
            fillMode == nil and "solid" or fillMode
        )
    end
end

---Sets the anti-aliasing sampling level for a rendering pipeline. 0
---(default) is the original hard-edge rendering, byte-for-byte unchanged.
---1+ supersamples edges for smoother lines/circles/filled polygons (mode
---"2d") or cube/mesh wireframe edges, points, and solid faces (mode "3d")
----- higher looks smoother, up to WindowService.GetMaxAliasingQuality();
---requests above that are silently clamped.
---@param mode AliasingMode
---@param quality integer?
---@return integer effectiveQuality The quality level actually used, after clamping
function WindowObject:SetAliasingQuality(mode, quality)
    if native_ok and type(window_interface) ~= "string" then
        return window_interface.set_aliasing_quality(self._id, mode, quality or 0)
    end
    return 0
end

---Gets the current anti-aliasing sampling level for a rendering pipeline.
---@param mode AliasingMode
---@return integer quality
function WindowObject:GetAliasingQuality(mode)
    if native_ok and type(window_interface) ~= "string" then
        return window_interface.get_aliasing_quality(self._id, mode)
    end
    return 0
end

-- -------------------------------------------------------------------------
-- CAMERA CREATION & MANAGEMENT METHODS
-- -------------------------------------------------------------------------

---Creates a new camera for positioning this window's 3D viewport. The
---camera is a standalone resource -- call SetActiveCamera to bind it.
---@param px number? Position X
---@param py number? Position Y
---@param pz number? Position Z
---@param fov number? Vertical field of view, in degrees (default 90)
---@param nearPlane number? Near clip plane (default 0.1)
---@param farPlane number? Far clip plane (default 100)
---@return CameraObject?
function WindowObject:CreateCamera(px, py, pz, fov, nearPlane, farPlane)
    return CameraObject.new(px, py, pz, fov, nearPlane, farPlane)
end

---Binds a camera to this window so DrawCube/DrawMesh render from its
---position/rotation/FOV.
---@param camera CameraObject
---@return nil
function WindowObject:SetActiveCamera(camera)
    if native_ok and type(window_interface) ~= "string" and camera then
        window_interface.set_active_camera(self._id, camera:GetId())
        self._activeCamera = camera
    end
end

---Gets the camera currently bound to this window's 3D viewport, if any.
---@return CameraObject?
function WindowObject:GetActiveCamera()
    return self._activeCamera
end

-- -------------------------------------------------------------------------
-- LIGHT CREATION & MANAGEMENT METHODS ("BASIC SHADERS")
-- -------------------------------------------------------------------------

---Creates a new directional light for basic per-face shading. Standalone
---resource -- call SetActiveLight to bind it.
---@param dx number? Light travel direction X
---@param dy number? Light travel direction Y (default -0.7, i.e. shining down)
---@param dz number? Light travel direction Z
---@param ambient number? Base brightness 0-1 (default 0.35)
---@param intensity number? Diffuse multiplier (default 1.0)
---@return LightObject?
function WindowObject:CreateLight(dx, dy, dz, ambient, intensity)
    return LightObject.new(dx, dy, dz, ambient, intensity)
end

---Binds a light to this window so solid-filled DrawCube/DrawMesh faces
---get flat per-face shading instead of a uniform flat color.
---@param light LightObject
---@return nil
function WindowObject:SetActiveLight(light)
    if native_ok and type(window_interface) ~= "string" and light then
        window_interface.set_active_light(self._id, light:GetId())
        self._activeLight = light
    end
end

---Gets the light currently bound to this window, if any.
---@return LightObject?
function WindowObject:GetActiveLight()
    return self._activeLight
end

-- -------------------------------------------------------------------------
-- PHYSICS
-- -------------------------------------------------------------------------
-- A GameObject is purely visual by default -- BindPhysics is the one call
-- that turns it into a real participant in the simulation: it creates a
-- PhysicsBody at the object's current position and links the two, so every
-- StepPhysics call afterward copies the body's simulated position back
-- onto the GameObject automatically (which then renders normally next
-- RenderAll). Skip BindPhysics and an object just sits there, rendered but
-- never touched by physics -- exactly the "won't interact with anything,
-- stays a visual object" behavior. A physics world is created lazily on
-- the first BindPhysics call if you haven't made one explicitly yet.

---Explicitly creates this window's physics world (useful if you want
---custom gravity before binding anything). If you skip this, BindPhysics
---creates one lazily with default gravity on first use.
---@param gx number? Gravity X (default 0)
---@param gy number? Gravity Y (default -9.81)
---@param gz number? Gravity Z (default 0)
---@return PhysicsWorld?
function WindowObject:CreatePhysicsWorld(gx, gy, gz)
    if not self._physicsWorld then
        self._physicsWorld = PhysicsModule.PhysicsWorld.new(gx, gy, gz)
    end
    return self._physicsWorld
end

---Gets this window's physics world, if one has been created (explicitly or via BindPhysics).
---@return PhysicsWorld?
function WindowObject:GetPhysicsWorld()
    return self._physicsWorld
end

---Binds a GameObject to this window's physics simulation, creating a
---PhysicsBody for it and linking the two. From now on, every StepPhysics
---call moves the body, spins it, and copies both the new position AND
---rotation onto `gameObject` -- so a rolling/tumbling body actually looks
---like it's rolling/tumbling once rendered. Without this call, a
---GameObject is never touched by physics no matter what else you do to it
----- it's just rendered where you put it.
---@param gameObject GameObject The object to drive physically; its current Position/Rotation seed the body's starting position/rotation
---@param options table? {shape="sphere"|"box"|"hull" (default "sphere"), radius=number (sphere, default 0.5), halfExtents={x,y,z} (box, default {0.5,0.5,0.5}), vertices=Vertex3[] (hull -- defaults to gameObject.Vertices, so this is optional when gameObject is a MeshObject), faces=MeshFace[] (hull -- defaults to gameObject.Faces, same MeshObject default as vertices; hull must be CONVEX, see PhysicsWorld:CreateHullBody), mass=number (default 1.0, ignored if density is given), density=number (alternative to mass -- mass = density * shape volume), isStatic=boolean (default false, i.e. "anchored" -- see PhysicsBody:SetStatic), restitution=number (bounciness, 0..1), friction=number (grip, 0..1 -- this is what makes a pushed/dropped body roll), damping=number (linear), angularDamping=number, group=integer (see PhysicsModule.PhysicsGroups), collidesWith=integer}
---@return PhysicsBody?
function WindowObject:BindPhysics(gameObject, options)
    if not gameObject then return nil end

    if not self._physicsWorld then
        self._physicsWorld = PhysicsModule.PhysicsWorld.new()
    end
    if not self._physicsWorld then return nil end

    options = options or {}

    local px, py, pz = gameObject:GetPosition()
    local body

    if options.shape == "hull" then
        -- options.vertices/faces let you pass an explicit convex hull;
        -- otherwise, for a MeshObject, its own Vertices/Faces are used
        -- automatically -- BindPhysics(meshObject, {shape = "hull"}) is
        -- all that's needed for the mesh to collide using its ACTUAL
        -- geometry (see PhysicsWorld:CreateHullBody).
        local vertices = options.vertices or gameObject.Vertices
        local faces = options.faces or gameObject.Faces
        body = self._physicsWorld:CreateHullBody(
            vertices, faces,
            px, py, pz,
            options.mass, options.isStatic
        )
    else
        local shapeParams
        if (options.shape or "sphere") == "box" then
            local he = options.halfExtents or {}
            shapeParams = { he[1] or 0.5, he[2] or 0.5, he[3] or 0.5 }
        else
            shapeParams = { options.radius or 0.5 }
        end

        body = self._physicsWorld:CreateBody(
            options.shape or "sphere", shapeParams,
            px, py, pz,
            options.mass, options.isStatic
        )
    end
    if not body then return nil end

    local rx, ry, rz = gameObject:GetRotation()
    body:SetRotation(rx, ry, rz)

    if options.density then body:SetDensity(options.density) end
    if options.restitution then body:SetRestitution(options.restitution) end
    if options.friction then body:SetFriction(options.friction) end
    if options.damping then body:SetDamping(options.damping) end
    if options.angularDamping then body:SetAngularDamping(options.angularDamping) end
    if options.group then body:SetGroup(options.group) end
    if options.collidesWith then body:SetCollidesWith(options.collidesWith) end

    table.insert(self._physicsLinks, { gameObject = gameObject, body = body })
    gameObject._physicsBody = body
    return body
end

---Unbinds a GameObject from physics: destroys its PhysicsBody and drops
---the link. The object goes back to being purely visual, staying wherever
---it last was.
---@param gameObject GameObject
function WindowObject:UnbindPhysics(gameObject)
    if not gameObject or not gameObject._physicsBody then return end

    gameObject._physicsBody:Destroy()
    gameObject._physicsBody = nil

    for i = #self._physicsLinks, 1, -1 do
        if self._physicsLinks[i].gameObject == gameObject then
            table.remove(self._physicsLinks, i)
        end
    end
end

---Gets the PhysicsBody a GameObject is bound to, if any.
---@param gameObject GameObject
---@return PhysicsBody?
function WindowObject:GetPhysicsBody(gameObject)
    return gameObject and gameObject._physicsBody
end

---Advances this window's physics world by `dt` seconds, syncs every bound
---GameObject's Position AND Rotation to its body's new simulated pose (the
---rotation half of that sync was missing before -- rolling/tumbling bodies
---were spinning in the physics world but rendering perfectly upright), and
---fires `Collided` on both PhysicsBody/GameObject of every pair still
---touching as of this Step -- keeps firing every Step for as long as the
---touch lasts, not just once when it starts. A no-op if no physics world
---exists yet (nothing has called BindPhysics or CreatePhysicsWorld).
---@param dt number
function WindowObject:StepPhysics(dt)
    if not self._physicsWorld then return end

    -- Step FIRST -- syncing pose before advancing the simulation would
    -- copy last frame's stale position/rotation instead of the new one.
    local touchingPairs = self._physicsWorld:Step(dt)

    -- Built while syncing pose below -- lets touchingPairs (native
    -- body ids) resolve back to both the PhysicsBody AND the GameObject
    -- (if any) whose Collided event needs to Fire.
    local bodyIdToObject = {}
    local bodyIdToBody = {}

    for _, link in ipairs(self._physicsLinks) do
        local x, y, z = link.body:GetPosition()
        link.gameObject:SetPosition(x, y, z)

        local rx, ry, rz = link.body:GetRotation()
        link.gameObject:SetRotation(rx, ry, rz)

        bodyIdToObject[link.body:GetId()] = link.gameObject
        bodyIdToBody[link.body:GetId()] = link.body
    end

    for _, pair in ipairs(touchingPairs) do
        -- Fires on both the PhysicsBody and the GameObject it's bound to
        -- -- every entry in _physicsLinks has both, since BindPhysics is
        -- the only way a link gets created here. A body made directly via
        -- PhysicsWorld:CreateBody (bypassing BindPhysics) isn't in
        -- _physicsLinks and won't have either Fire automatically.
        local bodyA = bodyIdToBody[pair[1]]
        local bodyB = bodyIdToBody[pair[2]]
        if bodyA and bodyB then
            bodyA.Collided:Fire(bodyB)
            bodyB.Collided:Fire(bodyA)
        end

        local objA = bodyIdToObject[pair[1]]
        local objB = bodyIdToObject[pair[2]]
        if objA and objB then
            objA.Collided:Fire(objB)
            objB.Collided:Fire(objA)
        end
    end
end

---Manual override: pins this window's physics quality level (0=Low ..
---3=Ultra) and disables automatic dynamic level shifting. No-op if no
---physics world exists yet.
---@param level PhysicsQualityLevel
---@return integer effectiveLevel
function WindowObject:SetPhysicsQuality(level)
    if not self._physicsWorld then return level end
    return self._physicsWorld:SetQuality(level)
end

---Re-enables (true) or disables (false) automatic dynamic physics quality
---shifting for this window's world. No-op if no physics world exists yet.
---@param enabled boolean
function WindowObject:SetPhysicsAutoQuality(enabled)
    if self._physicsWorld then
        self._physicsWorld:SetAutoQuality(enabled)
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

---Instantiates a modifiable TextObject extending GameObject.
---@param text string? String text content
---@param x number? Position X
---@param y number? Position Y
---@param scale integer? Font scaling factor
---@param r integer? Red component (0-255)
---@param g integer? Green component (0-255)
---@param b integer? Blue component (0-255)
---@param quality integer? Sampling/anti-aliasing level (default 0 = legacy blocky rendering; see WindowService.GetMaxTextQuality)
---@return TextObject
function WindowObject:CreateText(text, x, y, scale, r, g, b, quality)
    local obj = Objects.TextObject.new(text, x, y, scale, r, g, b, quality)
    table.insert(self._objects, obj)
    return obj
end

---Instantiates a modifiable PolygonObject extending GameObject.
---@param points table? Array of vertex coordinates
---@param fill boolean? Fill shape toggle
---@param r integer? Red component (0-255)
---@param g integer? Green component (0-255)
---@param b integer? Blue component (0-255)
---@return PolygonObject
function WindowObject:CreatePolygon(points, fill, r, g, b)
    local obj = Objects.PolygonObject.new(points, fill, r, g, b)
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
---@param fillMode CubeFillMode|boolean|integer? "wireframe"|"solid"|"point" (default "wireframe", matching this object's original default)
---@return CubeObject
function WindowObject:CreateCube(px, py, pz, size, r, g, b, fillMode)
    local obj = Objects.CubeObject.new(px, py, pz, size, r, g, b, fillMode)
    table.insert(self._objects, obj)
    return obj
end

---Instantiates a modifiable MeshObject extending GameObject -- the 3D
---equivalent of CreatePolygon. Supply your own vertex/face list.
---@param vertices Vertex3[]? Local-space vertex list
---@param faces MeshFace[]? Each face is a list of 1-based vertex indices (3+)
---@param r integer? Red component (0-255)
---@param g integer? Green component (0-255)
---@param b integer? Blue component (0-255)
---@param fillMode CubeFillMode|boolean|integer? "wireframe"|"solid"|"point" (default "solid")
---@return MeshObject
function WindowObject:CreateMesh(vertices, faces, r, g, b, fillMode)
    local obj = Objects.MeshObject.new(vertices, faces, r, g, b, fillMode)
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

---Gets the primary display's resolution. Not tied to any window --
---useful for centering a window or picking a sane default size before
---one exists.
---@return integer width
---@return integer height
function WindowService.GetDisplayResolution()
    if native_ok and type(window_interface) ~= "string" then
        local w, h = window_interface.get_display_resolution()
        return w or 0, h or 0
    end
    return 0, 0
end

---Gets the max text sampling/quality level DrawText will actually use --
---requests above this get silently clamped. Not tied to any window.
---@return integer maxQuality
function WindowService.GetMaxTextQuality()
    if native_ok and type(window_interface) ~= "string" then
        return window_interface.get_max_text_quality()
    end
    return 0
end

---Gets the max anti-aliasing sampling level SetAliasingQuality will
---actually use -- requests above this get silently clamped. Same cap for
---both "2d" and "3d" modes. Not tied to any window.
---@return integer maxQuality
function WindowService.GetMaxAliasingQuality()
    if native_ok and type(window_interface) ~= "string" then
        return window_interface.get_max_alias_quality()
    end
    return 0
end

---Closes all active managed windows.
---@return nil
function WindowService:CloseAll()
    for id, win in pairs(self._windows) do
        win:Close()
        self._windows[id] = nil
    end 
    self._activeWindow = nil
end

return WindowService
