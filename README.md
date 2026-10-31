# MaxCore REMASTERED

### Global Environment Variables

```lua
EngineCore: EngineCore|MaxCore
```

```lua
kpairs(t: table, step: boolean): function
```

```lua
os.native_execute(command: string, options: table): (sucess: boolean|integer|nil, error: string|nil)
```

> _**NOTE:**_ the above syntax 
> is not literal Lua syntax,
> it's just a type reference.

### Service Loading Reference

```lua
local ServiceObject = EngineCore:LoadService(name: string)
```

### Service Usage

### RunnerService

The `RunnerService`, also known as the runtime service, provides utilities for maintaining active runtime processes and binding runtime simulations to `deltaTime` events.

Functions passed through the `Connect` method are:
* `delta: number`
* `wait: function(s: number): number`
* `join: function(id: integer): TaskEvent`

**TIP:** The scheduler for the runtime service is sequenced through Stepped -> Hearbeat -> RenderStepped (which occurs last just before the frame is fully rendered.)

```lua
-- properties
RunnerService.Stepped -> RunnerSignal
RunnerService.Heartbeat -> RunnerSignal
RunnerService.RenderStepped -> RunnerSignal
```

```lua
-- signal methods
RunnerSignal.Connect: fun(thread: function|string|thread, options: JobOptions): RunnerConnection
-- connection methods
RunnerConnection.disconnect -> fun(self: RunnerConnection): nil
RunnerConnection.Pause -> fun(self: RunnerConnection): nil
RunnerConnection.Resume -> fun(self: RunnerConnection): nil
-- connection properties
RunnerConnection.Connected -> boolean
```

> The `Connect` method is expanded from the base RunnerSignal.

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

`InputService` provides input-related functionality, including global input control, and input simulation, etc.

(eg. can be used as creating an auto clicker for the application or outside the sandbox virtual machine.)

**Methods:**

```lua
InputService.SetGlobalInput -> fun(target: boolean): nil
InputService.GetGlobalInput -> fun(): 
InputService.Update -> fun(): nil -- updates input(s) bindings within the current environment
InputService.UpdateAll -> fun(): nil -- alternative to update, updates bindings outside the application
```

**Backend Wrapper Methods:**

```lua
InputService._ResolveTarget -> fun(target: string): (string?, integer)
```

> **Note:** The signatures above use type-annotation notation for documentation. They are not necessarily literal Lua call syntax.

## Building

MaxCore REMASTERED uses CMake for configuring and building the engine.

### Linux (Devbox)

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

### Windows / macOS / Other Linux Environments

Install CMake and the required platform development dependencies, then execute:

```bash
cmake -S . -B build
cmake --build build --parallel
```

### Experimental Runtime Metadata Generation

Experimental LuaLS metadata generation is disabled by default.

To enable it:

```bash
cmake -S . -B build \
    -DMAXCORE_EXPERIMENTAL_METADATA=ON

cmake --build build --parallel
```

The generated metadata is experimental and may not fully represent every API member or function signature. The existing MaxCore type definition remains the preferred source for API documentation and static analysis.
