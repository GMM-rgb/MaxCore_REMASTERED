# MaxCore REMASTERED — Edition

## Documentation

### Globala

```lua
EngineCore: EngineCore
```
> **NOTE:** the above syntax 
> is not literal Lua syntax,
> it's just type reference.

### CoreLib Importing

Import the core library and initialize the engine:

```lua
local core <const> = require("max_core").call()
```

### Service Loading

Load a service using the initialized core instance:

```lua
local storage = core:LoadService("StorageService")
```

## Service Usage

### RunnerService

The `RunnerService`, also known as the runtime service, provides utilities for maintaining active runtime processes and binding runtime simulations to `deltaTime` events.

**Methods:** *Documented methods coming soon.*

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
