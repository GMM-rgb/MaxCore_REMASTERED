import electron, { BrowserWindow, ipcMain } from "electron";
import { MaxCoreCommunication } from "./engine/communication";
import { ChildProcessWithoutNullStreams } from "child_process";
import * as path from "path";

type ChildProcessNull = ChildProcessWithoutNullStreams;
let EditorWindow: Electron.BrowserWindow | null = null;

function main(lua: ChildProcessNull): void {
    EditorWindow = new BrowserWindow({
        width: 400, height: 400, hasShadow: true, center: true, focusable: true, roundedCorners: true,
        webPreferences: { preload: path.join(__dirname, 'preload.js'), contextIsolation: true, nodeIntegration: false },
    });

    ipcMain.handle("engine:send-command", async (_event, cmd: string): Promise<object> => {
        console.assert(!cmd, "invalid command argument for engine invoke!");
        return { status: "ok" };
    });

    ipcMain.handle("engine:execute-script", async (_event, content: string) => {
        if (!lua || !lua.stdin) return; lua.stdin.write(content + "\n");
        return { status: 'error', message: 'interpreter not running' };
    });
}

let lua = MaxCoreCommunication.start_lua_machine();
electron.app.whenReady().then(() => { main(lua); console.debug("initalizing..."); });
electron.app.on("window-all-closed", () => electron.app.quit);
