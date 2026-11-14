var _a, _b;
import { EditorToolbarMenu } from "./toolbar_menu_instancer.js";
var InterfaceElements = { "ToolbarInterface": document.getElementById("editor-toolbar") };
var ToolbarButtons = (_b = (_a = InterfaceElements.ToolbarInterface) === null || _a === void 0 ? void 0 : _a.querySelectorAll("button")) !== null && _b !== void 0 ? _b : [];
var ToolbarElement = InterfaceElements["ToolbarInterface"];
ToolbarButtons.forEach(function (button) {
    var _a;
    InterfaceElements[String(button.name)] = button;
    var ButtonMenu = new EditorToolbarMenu(button, ToolbarElement);
    var MenuElement = (_a = ButtonMenu === null || ButtonMenu === void 0 ? void 0 : ButtonMenu.construct_menu()) !== null && _a !== void 0 ? _a : void null;
    ButtonMenu.add_commands();
    ButtonMenu.toggle(false);
});
