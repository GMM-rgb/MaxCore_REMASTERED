import { EditorToolbarMenu } from "./toolbar_menu_instancer.js";
import { BlockBuilderMachine } from "./components/block_constructor.js";
import { VirtualMachine, VirtualMachineParser } from"./components/blbvm.js";

let InterfaceElements: Record<string, HTMLElement | null | undefined> = { "ToolbarInterface": document.getElementById("editor-toolbar") };
let ToolbarButtons = InterfaceElements.ToolbarInterface?.querySelectorAll("button") ?? [];
const ToolbarElement = InterfaceElements["ToolbarInterface"] as HTMLDivElement;
const EditorLuaVM = new VirtualMachine.Machine();

ToolbarButtons.forEach((button: HTMLButtonElement) => {
    InterfaceElements[String(button.name)] = button;
    const ButtonMenu = new EditorToolbarMenu(button, ToolbarElement);
    const MenuElement = ButtonMenu?.construct_menu() ?? void null;
    ButtonMenu.add_commands<HTMLButtonElement>();
    ButtonMenu.toggle(false); // ensure closed
});

const PrintGeometry: BlockBuilderMachine.BlockGeometry = {
    radius: 2, bottom_connector: true, top_notch: true,
    notch: { depth: 6, width: 12, offset: 13 },
};

const PrintCommandConstructor = new BlockBuilderMachine.Block({
    fill: "#505eb0", font: "12px Arial", geometry: PrintGeometry,
}, BlockBuilderMachine.CommandBlocks.COM, "<em>{f}</em><strong>print</strong>");

PrintCommandConstructor.add_param({
    content: "...", width: void null,
    type: BlockBuilderMachine.InputTypes.STRING,
});

EditorLuaVM.inject_block(PrintCommandConstructor);
const PrintCommand = PrintCommandConstructor.invoke();
document.body.appendChild(PrintCommand);
