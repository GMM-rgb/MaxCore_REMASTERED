# MaxCore REMASTERED

## Global Environment Variables

```lua
EngineCore: EngineCore|MaxCore
```

```lua
kpairs(t: table, step: boolean): function
```

```lua
os.native_execute(command: string, options: table): (success: boolean|integer|nil, error: string|nil)
```

> **NOTE:** The syntax above is not literal Lua syntax. It is used exclusively for type references and documentation.

## Service Loading Reference

```lua
local ServiceObject = EngineCore:LoadService(name: string)
```

## Service Usage

### RunnerService

The `RunnerService`, also known as the runtime service, provides utilities for maintaining active runtime processes and binding runtime simulations to `deltaTime` events.

Functions passed through the `Connect` method receive the following arguments:

- `delta: number`
- `wait: function(s: number): number`
- `join: function(id: integer): TaskEvent`

**TIP:** The scheduler for the runtime service follows this execution sequence:

`Stepped` → `Heartbeat` → `RenderStepped`

`RenderStepped` occurs last, immediately before the frame is rendered.

**Properties:**

```lua
RunnerService.Stepped -> RunnerSignal
RunnerService.Heartbeat -> RunnerSignal
RunnerService.RenderStepped -> RunnerSignal
```

**Signal methods:**

```lua
RunnerSignal.Connect: fun(thread: function|string|thread, options: JobOptions): RunnerConnection
```

**Connection methods:**

```lua
RunnerConnection.disconnect -> fun(self: RunnerConnection): nil
RunnerConnection.Pause -> fun(self: RunnerConnection): nil
RunnerConnection.Resume -> fun(self: RunnerConnection): nil
```

**Connection properties:**

```lua
RunnerConnection.Connected -> boolean
```

> The `Connect` method is expanded from the base `RunnerSignal` implementation.

---

### SoundService

`SoundService` provides native integration with the operating system and audio hardware.

**Methods:** *Documented methods coming soon.*

---

### StorageService

`StorageService` provides storage-related functionality within the engine.

**Methods:** *Documented methods coming soon.*

---

### WindowService

`WindowService` provides window-management functionality.

**Methods:** *Documented methods coming soon.*

---

### InputService

`InputService` provides input-related functionality, including global input control and input simulation.

For example, it can be used to implement automated input within an application or interact with input outside the sandbox virtual machine.

**Methods:**

```lua
InputService.SetGlobalInput -> fun(target: boolean): nil
InputService.GetGlobalInput -> fun()
InputService.Update -> fun(): nil -- Updates input bindings within the current environment
InputService.UpdateAll -> fun(): nil -- Updates bindings outside the application
```

**Backend Wrapper Methods:**

```lua
InputService._ResolveTarget -> fun(target: string): (string?, integer)
```

> **NOTE:** The signatures above use type-annotation notation for documentation. They are not necessarily literal Lua call syntax.

---

# Building

MaxCore REMASTERED uses CMake for configuring and building the engine.

## Linux (Devbox)

Linux development uses [Devbox](https://www.jetify.com/devbox) to provide a reproducible build environment with the required compiler toolchain and X11 development dependencies.

**Requirements:**
- [Devbox](https://www.jetify.com/devbox)
- Git

**Build process:**

```bash
# Initialize the development environment
devbox install

# Configure and compile the project
devbox run build
```

Alternatively, enter the development environment manually:

```bash
devbox shell

cmake -S . -B build
cmake --build build --parallel
```

## Windows

Install CMake, a compatible C++ compiler toolchain, and the required Windows development dependencies.

Then configure and build:

```bash
cmake -S . -B build
cmake --build build --parallel
```

## macOS

MaxCore supports macOS through its native platform backend.

**Requirements:**
- [Homebrew](https://brew.sh/)
- CMake
- Xcode Command Line Tools

### Vulkan Support

macOS does not provide native Vulkan support. MaxCore uses [MoltenVK](https://github.com/KhronosGroup/MoltenVK) to provide Vulkan compatibility through Apple's Metal graphics API.

Install the required Vulkan packages using Homebrew:

```bash
brew install vulkan-loader molten-vk vulkan-tools
```

**Package descriptions:**

- `vulkan-loader` — Provides the Vulkan runtime loader.
- `molten-vk` — Provides Vulkan support through Apple's Metal API.
- `vulkan-tools` — Provides Vulkan diagnostic utilities, including `vulkaninfo`.

**Verify the installation:**

```bash
vulkaninfo --summary
```

The output should list an available physical GPU and its Vulkan driver. On Apple Silicon systems, the GPU should be identified through MoltenVK.

**Build MaxCore:**

```bash
cmake -S . -B build
cmake --build build --parallel
```

> **NOTE:** Vulkan support on macOS requires a working Vulkan loader and MoltenVK installation. If Vulkan initialization fails, MaxCore may fall back to its CPU rasterizer.

## Other Linux Environments

For Linux environments outside Devbox, install CMake, a compatible C++ compiler toolchain, and the required X11 development libraries.

Then configure and build:

```bash
cmake -S . -B build
cmake --build build --parallel
```

> **NOTE:** Linux builds require X11 development dependencies for the native window backend.

---

## Experimental Runtime Metadata Generation

Experimental LuaLS metadata generation is disabled by default.

To enable it:

```bash
cmake -S . -B build \
    -DMAXCORE_EXPERIMENTAL_METADATA=ON

cmake --build build --parallel
```

The generated metadata is experimental and may not fully represent every API member or function signature.

The existing MaxCore type definition remains the preferred source for API documentation and static analysis.
