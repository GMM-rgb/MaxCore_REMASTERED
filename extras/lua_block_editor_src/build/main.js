import { EditorToolbarMenu } from "./toolbar_menu_instancer.js";
import { BlockBuilderMachine } from "./components/block_constructor.js";
import { VirtualMachine } from "./components/blbvm.js";
let InterfaceElements = { "ToolbarInterface": document.getElementById("editor-toolbar") };
let ToolbarButtons = InterfaceElements.ToolbarInterface?.querySelectorAll("button") ?? [];
const ToolbarElement = InterfaceElements["ToolbarInterface"];
const EditorLuaVM = new VirtualMachine.Machine();
ToolbarButtons.forEach((button) => {
    InterfaceElements[String(button.name)] = button;
    const ButtonMenu = new EditorToolbarMenu(button, ToolbarElement);
    const MenuElement = ButtonMenu?.construct_menu() ?? void null;
    ButtonMenu.add_commands();
    ButtonMenu.toggle(false);
});
const PrintGeometry = {
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
