"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
const electron_1 = __importStar(require("electron"));
const communication_1 = require("./engine/communication");
const path = __importStar(require("path"));
const fs = __importStar(require("fs"));
let EditorWindow = null;
function main(lua) {
    EditorWindow = new electron_1.BrowserWindow({
        width: 400, height: 400, hasShadow: true, center: true, focusable: true, roundedCorners: true,
        webPreferences: { preload: path.join(__dirname, 'preload.js'), contextIsolation: true, nodeIntegration: false },
    });
    electron_1.ipcMain.handle("engine:send-command", async (_event, cmd) => {
        console.assert(!cmd, "invalid command argument for engine invoke!");
        return { status: "ok" };
    });
    electron_1.ipcMain.handle("engine:execute-script", async (_event, content) => {
        if (!lua || !lua.stdin)
            return;
        lua.stdin.write(content + "\n");
        return { status: 'error', message: 'interpreter not running' };
    });
    if (EditorWindow !== null)
        EditorWindow.loadFile(__dirname + "./../../src/index.html");
    let HandleBuffer = EditorWindow.getNativeWindowHandle();
    let WindowHandle = HandleBuffer.readBigUInt64LE(undefined);
    const ApplicationToolbar = electron_1.Menu.buildFromTemplate([{
            label: "File", submenu: [{
                    label: "open", click: async () => { await LoadFile(); },
                }]
        }]);
    electron_1.Menu.setApplicationMenu(ApplicationToolbar);
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
const lua = communication_1.MaxCoreCommunication.start_lua_machine();
console.debug("machine:", lua);
electron_1.default.app.whenReady().then(() => {
    console.debug("initializing...");
    main(lua);
    const scriptPath = path.join(__dirname, "../../../world.lua");
    console.debug("script path:", scriptPath, "exists:", fs.existsSync(scriptPath));
    const content = fs.readFileSync(scriptPath, "utf-8");
    console.debug("content bytes:", content.length);
    const testRes = communication_1.MaxCoreCommunication.send_command(lua, 'print("LUA_ALIVE")');
    console.debug("test res:", testRes);
    const worldRes = communication_1.MaxCoreCommunication.send_command(lua, content);
    console.debug("world res:", worldRes);
});
electron_1.default.app.on("window-all-closed", () => {
    if (process.platform !== "darwin") {
        electron_1.default.app.quit();
    }
});
//# sourceMappingURL=app.js.map