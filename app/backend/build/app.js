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
let EditorWindow = null;
EditorWindow = new electron_1.BrowserWindow({
    width: 400,
    height: 400,
    hasShadow: true,
    center: true,
    webPreferences: {
        sandbox: false,
        preload: path.join(__dirname, 'preload.js'),
        contextIsolation: true,
        nodeIntegration: false,
    },
});
function main(lua) {
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
}
let lua = communication_1.MaxCoreCommunication.start_lua_machine();
electron_1.default.app.whenReady().then(() => { main(lua); });
electron_1.default.app.on("window-all-closed", () => electron_1.default.app.quit);
//# sourceMappingURL=app.js.map