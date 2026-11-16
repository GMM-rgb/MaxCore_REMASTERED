import { EditorToolbarMenu } from "./toolbar_menu_instancer.js";
import { BlockBuilderMachine } from "./components/block_constructor.js";
let InterfaceElements = { "ToolbarInterface": document.getElementById("editor-toolbar") };
let ToolbarButtons = InterfaceElements.ToolbarInterface?.querySelectorAll("button") ?? [];
const ToolbarElement = InterfaceElements["ToolbarInterface"];
const BlockWorkspace = document.body.querySelector("#block-workspace-container");
ToolbarButtons.forEach((button) => {
    InterfaceElements[String(button.name)] = button;
    const ButtonMenu = new EditorToolbarMenu(button, ToolbarElement);
    ButtonMenu?.construct_menu() ?? void null;
    ButtonMenu.add_commands();
    ButtonMenu.toggle(false);
});
const CommandGeometry = {
    radius: 2, bottom_connector: true, top_notch: true,
    notch: { depth: 6, width: 12, offset: 13 },
};
const ReporterGeometry = {
    radius: 14, bottom_connector: false, top_notch: false,
    notch: { depth: 0, width: 0, offset: 0 },
};
const ToolboxPalette = [
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
async function setup_toolbox() {
    return new Promise((resolve) => {
        let blocks = [];
        for (const selected of ToolboxPalette) {
            const { type, text, params } = selected.options;
            let block = new BlockBuilderMachine.Block(selected.config, type, text);
            BlockWorkspace?.appendChild(block.invoke());
            if (params !== undefined) {
                params.forEach((param) => {
                    block.add_param(param);
                });
            }
            blocks.push(block);
        }
        resolve(blocks);
    });
}
(async () => {
    const ToolboxBlocks = await setup_toolbox();
    console.debug(ToolboxBlocks);
})();
