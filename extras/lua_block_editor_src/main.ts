import { EditorToolbarMenu } from "./toolbar_menu_instancer.js";
import { BlockBuilderMachine } from "./components/block_constructor.js";

let InterfaceElements: Record<string, HTMLElement | null | undefined> = { "ToolbarInterface": document.getElementById("editor-toolbar") };
let ToolbarButtons = InterfaceElements.ToolbarInterface?.querySelectorAll("button") ?? [];
let ToolbarElement = InterfaceElements["ToolbarInterface"] as HTMLDivElement;

ToolbarButtons.forEach((button: HTMLButtonElement) => {
    InterfaceElements[String(button.name)] = button;
    const ButtonMenu = new EditorToolbarMenu(button, ToolbarElement);
    const MenuElement = ButtonMenu?.construct_menu() ?? void null;
    ButtonMenu.add_commands<HTMLButtonElement>();
    ButtonMenu.toggle(false); // ensure closed
});
