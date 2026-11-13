import child from "child_process";
import * as path from "path";
export var MaxCoreCommunication;
(function (MaxCoreCommunication) {
    function start_lua_machine() {
        const machine_path = path.join(__dirname, "./../../../../build/max-core-lua.exe");
        const args = ["-"];
        const lua_process = child.spawn(machine_path, args, {
            stdio: ["pipe", "pipe", "pipe"]
        });
        lua_process.on("error", (err) => {
            console.error("[MEC] Failed to start Lua Machine:", err);
        });
        lua_process.stdout.on("data", (data) => {
            console.log(`[MEC (MaxCore Engine Components) Output]: ${data.toString()}`);
        });
        lua_process.stderr.on("data", (data) => {
            console.error(`[MEC (MaxCore Engine Components) Error]: ${data.toString()}`);
        });
        lua_process.on("exit", (code, signal) => {
            console.log(`[MEC] Lua Machine exited with code ${code} (signal: ${signal})`);
        });
        const isRunning = lua_process.pid !== undefined && lua_process.exitCode === null;
        console.debug(`STARTED: Lua Machine (PID: ${lua_process.pid})...\t${isRunning}`);
        return lua_process;
    }
    MaxCoreCommunication.start_lua_machine = start_lua_machine;
    function send_command(proc, code) {
        if (!proc.stdin.writable) {
            console.error("[MEC] Cannot write: stdin stream is closed.");
            return false;
        }
        console.info(code);
        return proc.stdin.write(`${code}\n`);
    }
    MaxCoreCommunication.send_command = send_command;
})(MaxCoreCommunication || (MaxCoreCommunication = {}));
//# sourceMappingURL=communication.js.map