import { EditorToolbarMenu } from "./toolbar_menu_instancer.js";
import { BlockBuilderMachine } from "./components/block_constructor.js";
import { VirtualMachine, VirtualMachineParser } from"./components/blbvm.js";

let InterfaceElements: Record<string, HTMLElement | null | undefined> = { "ToolbarInterface": document.getElementById("editor-toolbar") };
let ToolbarButtons = InterfaceElements.ToolbarInterface?.querySelectorAll("button") ?? [];
const ToolbarElement = InterfaceElements["ToolbarInterface"] as HTMLDivElement;
const BlockWorkspace = document.body.querySelector("#block-workspace-container");

ToolbarButtons.forEach((button: HTMLButtonElement) => {
    InterfaceElements[String(button.name)] = button;
    const ButtonMenu = new EditorToolbarMenu(button, ToolbarElement);
    ButtonMenu?.construct_menu() ?? void null;
    ButtonMenu.add_commands<HTMLButtonElement>();
    ButtonMenu.toggle(false); // ensure closed
});

const CommandGeometry: BlockBuilderMachine.BlockGeometry = {
    radius: 2, bottom_connector: true, top_notch: true,
    notch: { depth: 6, width: 12, offset: 13 },
};

const ToolboxPalette: Array<{config: BlockBuilderMachine.BlockConfig, options: { type: BlockBuilderMachine.OutputBlockTypes, text: string }}> = [
    {
        config: { fill: "#5486e2", font: "12px Arial", geometry: CommandGeometry },
        options: { type: BlockBuilderMachine.CommandBlocks.HAT, text: "<em>{global}</em><strong>EngineCore</strong>" },
    },
];

async function setup_toolbox(): Promise<BlockBuilderMachine.Block[]> {
    return new Promise((resolve) => {
        for (const selected of ToolboxPalette) {
            const { type, text } = selected.options;
            const block = new BlockBuilderMachine.Block(selected.config, type, text);
        }
    });
}

const PrintCommandConstructor = new BlockBuilderMachine.Block({
    fill: "#505eb0", font: "12px Arial", geometry: CommandGeometry,
}, BlockBuilderMachine.CommandBlocks.COM, "<em>{f}</em><strong>print</strong>");

PrintCommandConstructor.add_param({
    content: "...", width: void null,
    type: BlockBuilderMachine.InputTypes.STRING,
});

const PrintCommand = PrintCommandConstructor.invoke();
BlockWorkspace?.appendChild(PrintCommand);
