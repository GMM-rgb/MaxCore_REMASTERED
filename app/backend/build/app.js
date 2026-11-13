import electron, { BrowserWindow, ipcMain, Menu } from "electron";
import { MaxCoreCommunication } from "./engine/communication";
import * as path from "path";
import * as fs from "fs";
let EditorWindow = null;
function main(lua) {
    EditorWindow = new BrowserWindow({
        width: 400, height: 400, hasShadow: true, center: true, focusable: true, roundedCorners: true,
        webPreferences: { preload: path.join(__dirname, 'preload.js'), contextIsolation: true, nodeIntegration: false },
    });
    ipcMain.handle("engine:send-command", async (_event, cmd) => {
        console.assert(!cmd, "invalid command argument for engine invoke!");
        return { status: "ok" };
    });
    ipcMain.handle("engine:execute-script", async (_event, content) => {
        if (!lua || !lua.stdin)
            return;
        lua.stdin.write(content + "\n");
        return { status: 'error', message: 'interpreter not running' };
    });
    if (EditorWindow !== null)
        EditorWindow.loadFile(__dirname + "./../../src/index.html");
    let HandleBuffer = EditorWindow.getNativeWindowHandle();
    let WindowHandle = HandleBuffer.readBigUInt64LE(undefined);
    const ApplicationToolbar = Menu.buildFromTemplate([{
            label: "File", submenu: [{
                    label: "open", click: async () => { await LoadFile(); },
                }]
        }]);
    Menu.setApplicationMenu(ApplicationToolbar);
}
async function LoadFile() {
    let FileBufferContents = undefined;
    new Promise((resolve) => {
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = "*.lua";
        input.multiple = false;
        input.onchange = () => { resolve(input.files); };
        input.oncancel = () => { resolve(null); };
        input.click();
    }).then(async (files = null) => {
        if (files instanceof FileList) {
            for (let index = 0; index < files.length; index++) {
                const file = files?.item(index);
                FileBufferContents = await file?.text();
            }
        }
    });
    return FileBufferContents ?? `EngineCore.ColorPrint("ERROR", "loading project file failed!")`;
}
const lua = MaxCoreCommunication.start_lua_machine();
console.debug("machine:", lua);
electron.app.whenReady().then(() => {
    console.debug("initializing...");
    main(lua);
    const scriptPath = path.join(__dirname, "../../../world.lua");
    console.debug("script path:", scriptPath, "exists:", fs.existsSync(scriptPath));
    const content = fs.readFileSync(scriptPath, "utf-8");
    console.debug("content bytes:", content.length);
    const testRes = MaxCoreCommunication.send_command(lua, 'print("LUA_ALIVE")');
    console.debug("test res:", testRes);
    const worldRes = MaxCoreCommunication.send_command(lua, content);
    console.debug("world res:", worldRes);
});
electron.app.on("window-all-closed", () => {
    if (process.platform !== "darwin") {
        electron.app.quit();
    }
});
//# sourceMappingURL=app.js.map