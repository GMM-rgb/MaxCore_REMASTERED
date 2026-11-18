import { EditorToolbarMenu } from "./toolbar_menu_instancer.js";
import { BlockBuilderMachine, BlockWorkspaceEditor } from "./components/block_constructor.js";
import { CommandButtonSubmenu } from './toolbar_menu_instancer';
import { VirtualMachine, VirtualMachineParser } from"./components/blbvm.js";

let InterfaceElements: Record<string, HTMLElement | null | undefined> = { "ToolbarInterface": document.getElementById("editor-toolbar") };
let ToolbarButtons = InterfaceElements.ToolbarInterface?.querySelectorAll("button") ?? [];
const ToolbarElement = InterfaceElements["ToolbarInterface"] as HTMLDivElement;
const BlockWorkspace = document.body.querySelector("#block-workspace-container");
const BlockToolbox = document.body.querySelector(".block-palette-toolbox");

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
        config: {
            fill: "#fcbe03",
            font: "12px Arial, monospace",
            geometry: CommandGeometry,
            lua_code: `loadstring({0})`,
            template: true,
        },
        options: {
            type: BlockBuilderMachine.CommandBlocks.COM,
            text: "<em>{manual}</em><strong>eval</strong>",
            params: [
                {
                    content: `print("testing")`,
                    type: BlockBuilderMachine.InputTypes.STRING,
                },
            ],
        },
    },
    {
        config: {
            fill: "#505eb0",
            font: "12px Arial, monospace",
            lua_code: "print({0})",
            geometry: CommandGeometry,
            template: true,
        },
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
        config: {
            fill: "#30c646",
            font: "12px Arial, monospace",
            geometry: ReporterGeometry,
            lua_code: "string.format({0}, {1})",
            template: true,
        },
        options: {
            type: BlockBuilderMachine.CommandBlocks.REP,
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
        config: {
            fill: "#30c646",
            font: "12px Arial, monospace",
            geometry: ReporterGeometry,
            template: true,
        },
        options: {
            type: BlockBuilderMachine.CommandBlocks.REP,
            text: "<em>{lfunc}</em><strong>string.join</strong>",
            params: [
                {
                    content: "MaxCore is ", width: void null,
                    type: BlockBuilderMachine.InputTypes.STRING,
                },
                {
                    content: "cool!", width: void null,
                    type: BlockBuilderMachine.InputTypes.STRING,
                },
            ],
        },
    },
    {
        config: { fill: "#5486e2", font: "12px Arial, monospace", geometry: ReporterGeometry, template: true },
        options: { text: "<em>{bool}</em><strong>EngineCore Online?</strong>", type: BlockBuilderMachine.CommandBlocks.REP },
    },
    {
        config: {
            fill: "#5486e2",
            font: "12px Arial, monospace",
            geometry: ReporterGeometry,
            lua_code: "_G.EngineCore",
            template: true,
        },
        options: { type: BlockBuilderMachine.CommandBlocks.REP, text: "<em>{global}</em><strong>EngineCore</strong>" },
    },
];

async function setup_toolbox(): Promise<BlockBuilderMachine.Block[]> {
    return new Promise((resolve) => {
        let blocks: Array<BlockBuilderMachine.Block> = [];

        for (const selected of ToolboxPalette) {
            const { type, text, params } = selected.options;
            let block = new BlockBuilderMachine.Block(selected.config, type, text);
            BlockToolbox?.appendChild(block.invoke()); console.debug();

            if (params !== undefined) {
                params.forEach((param: BlockBuilderMachine.BlockInput) => { block.add_param(param); });
            }

            BlockBuilderMachine.VM.inject_block(block);
            block.remove_connectors();
            blocks.push(block);
        }

        resolve(blocks);
    });
}

(async (): Promise<void> => {
    if (!(BlockWorkspace instanceof HTMLElement)) return;
    BlockWorkspaceEditor.apply_editor_depth(BlockWorkspace, { debug: false, box_size_outline: false });
    BlockWorkspaceEditor.set_workspace(BlockWorkspace);
    const ToolboxBlocks = await setup_toolbox();
    console.debug(ToolboxBlocks);
})();

Object.assign(window, {
    debug_blocks: function(): void { 
        BlockBuilderMachine.VM.get_blocks().forEach((block: VirtualMachine.BlockDefinition) => {
            console.debug(block);
        });
    },
});
