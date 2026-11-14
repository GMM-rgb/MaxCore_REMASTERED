import { EditorToolbarMenu } from "./toolbar_menu_instancer.js";
let InterfaceElements = { "ToolbarInterface": document.getElementById("editor-toolbar") };
let ToolbarButtons = InterfaceElements.ToolbarInterface?.querySelectorAll("button") ?? [];
let ToolbarElement = InterfaceElements["ToolbarInterface"];
ToolbarButtons.forEach((button) => {
    InterfaceElements[String(button.name)] = button;
    const ButtonMenu = new EditorToolbarMenu(button, ToolbarElement);
    const MenuElement = ButtonMenu?.construct_menu() ?? void null;
    ButtonMenu.add_commands();
});
