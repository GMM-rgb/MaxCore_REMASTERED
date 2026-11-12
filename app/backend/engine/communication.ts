import * as process from "process";
import child, { ChildProcessWithoutNullStreams } from "child_process";
import * as path from "path";

export namespace MaxCoreCommunication {
    export function start_lua_machine(): ChildProcessWithoutNullStreams {
        const machine_path: string = path.join(__dirname, "../../../build/max-core-lua.exe");
        
        let lua_process = child.spawn(machine_path, [], {stdio: ['pipe', 'pipe', 'pipe']});
        // replace with a dedicated logger in future project build(s)
        lua_process.stdout?.on('data', (data) => { console.log(`[MEC (MaxCore Engine Components) Output]: ${data.toString()}`); });
        lua_process.stderr?.on('data', (data) => { console.error(`[MEC (MaxCore Engine Components) Error]: ${data.toString()}`); });

        return lua_process;
    }
}
