# MaxCore REMASTERED — Edition

## Documentation

### Globals

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

## Service Usage

### RunnerService

The `RunnerService`, also known as the runtime service, provides utilities for maintaining active runtime processes and binding runtime simulations to `deltaTime` events.

**Methods:** *Documented methods coming soon.*

```lua
-- properties
RunnerService.Stepped -> RunnerSignal
RunnerService.Heartbeat -> RunnerSignal
RunnerService.RenderStepped -> RunnerSignal
```

```lua
-- signal methods
RunnerSignal.Connect: fun(thread: function|string|thread, options: JobOptions): RunnerConnection
```

> The `Connect` method is expanded from the base RunnerSignal.

### SoundService

`SoundService` provides native integration with the operating system and audio hardware.

**Methods:** *Documented methods coming soon.*

### StorageService

`StorageService` provides storage-related functionality within the engine.

**Methods:** *Documented methods coming soon.*

### WindowService

`WindowService` provides window-management functionality.

**Methods:** *Documented methods coming soon.*

### InputService

`InputService` provides input-related functionality, including global input control.

**Methods:**

```lua
InputService:SetGlobalInput(target: boolean): void
InputService:GetGlobalInput(): boolean
```

> **Note:** The signatures above use type-annotation notation for documentation. They are not necessarily literal Lua call syntax.

## Building

Configure and build the project using CMake:

```bash
cmake -S . -B build
cmake --build build
```

## Extras

### Experimental Runtime Metadata Generation

MaxCore includes an experimental option for generating LuaLS metadata from the live `EngineCore` table.

This feature is disabled by default and is not required for normal engine operation.

> To Enable:

```bash
cmake -S . -B build -DMAXCORE_EXPERIMENTAL_METADATA=ON
cmake --build build
```

The generated metadata is experimental and may not fully represent every API member or function signature. The existing `MaxCore` type definition remains the preferred source for API documentation and static analysis.
