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

const PrintGeometry: BlockBuilderMachine.BlockGeometry = {
    radius: 5, bottom_connector: true, top_notch: true,
    notch: { depth: 8, width: 14 },
};

const PrintCommand = new BlockBuilderMachine.Block({
    fill: "#505eb0", font: "12px Arial", geometry: PrintGeometry,
}, BlockBuilderMachine.CommandBlocks.COM, "<strong>print</strong>").invoke();

document.body.appendChild(PrintCommand);
