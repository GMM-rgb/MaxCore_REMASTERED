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

const ReporterGeometry: BlockBuilderMachine.BlockGeometry = {
    radius: 14, bottom_connector: false, top_notch: false,
    notch: { depth: 0, width: 0, offset: 0 },
};

const ToolboxPalette: Array<{config: BlockBuilderMachine.BlockConfig, op?: Function, options: { type: BlockBuilderMachine.OutputBlockTypes, text: string, params?: Array<BlockBuilderMachine.BlockInput> }}> = [
    {
        config: { fill: "#5486e2", font: "12px Arial, monospace", geometry: CommandGeometry },
        options: { type: BlockBuilderMachine.CommandBlocks.HAT, text: "<em>{global}</em><strong>EngineCore</strong>" },
    },
    {
        config: { fill: "#505eb0", font: "12px Arial, monospace", geometry: CommandGeometry },
        options: {
            type: BlockBuilderMachine.CommandBlocks.COM,
            text: "<em>{func}</em><strong>print</strong>",
            params: [
                {
                    content: "...", width: void null,
                    type: BlockBuilderMachine.InputTypes.STRING
                },
            ],
        },
    },
    {
        config: { fill: "#30c646", font: "12px Arial, monospace", geometry: CommandGeometry },
        options: {
            type: BlockBuilderMachine.CommandBlocks.COM,
            text: "<em>{lfunc}</em><strong>string.format</strong>",
            params: [
                {
                    content: "test %s", width: void null,
                    type: BlockBuilderMachine.InputTypes.STRING,
                },
                {
                    content: "string", width: void null,
                    type: BlockBuilderMachine.InputTypes.STRING,
                },
            ],
        },
    },
    {
        config: { fill: "#5486e2", font: "12px Arial, monospace", geometry: ReporterGeometry },
        options: { text: "<em>{bool}</em><strong>EngineCore Online?</strong>", type: BlockBuilderMachine.CommandBlocks.END },
    },
];

async function setup_toolbox(): Promise<BlockBuilderMachine.Block[]> {
    return new Promise((resolve) => {
        let blocks: Array<BlockBuilderMachine.Block> = [];

        for (const selected of ToolboxPalette) {
            const { type, text, params } = selected.options;
            let block = new BlockBuilderMachine.Block(selected.config, type, text);
            BlockWorkspace?.appendChild(block.invoke());
            
            if (params !== undefined) {
                params.forEach((param: BlockBuilderMachine.BlockInput) => {
                    block.add_param(param);
                });
            }

            blocks.push(block);
        }

        resolve(blocks);
    });
}

(async (): Promise<void> => {
    const ToolboxBlocks = await setup_toolbox();
    console.debug(ToolboxBlocks);
})();
