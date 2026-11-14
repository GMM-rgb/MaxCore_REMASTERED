export class EditorToolbarMenu {
    TargetButton;
    ToolbarElement;
    MenuElement;
    MenuTemplate;
    static NextAnchorId = 0;
    MenuAnchorName = null;
    static ToolbarButtonCommands = {
        "file": [
            {
                "label": "<strong>Save Project</strong>",
                "op": () => { },
            },
            {
                "label": "<strong>Save File</strong>",
                "op": null,
            },
            {
                "label": "<strong>Load Project</strong>",
                "op": null,
            },
            {
                "label": "<strong>EXIT</strong>",
                "op": () => { window.close(); },
            },
        ],
    };
    apply_listener() {
        this?.TargetButton?.addEventListener("click", (e) => {
            console.debug("CLICKED TOOLBAR BUTTON:", this.TargetButton?.name ?? undefined);
        }, { passive: true });
    }
    initialize_menu() {
        if (this.TargetButton) {
            switch (this.TargetButton.name) {
                case "file":
                    this.apply_listener();
                    break;
                case "edit":
                    this.apply_listener();
                    break;
                case "settings":
                    this.apply_listener();
                    break;
                default:
                    console.warn("unkown button name");
                    return;
            }
        }
    }
    construct_menu() {
        if (!this.TargetButton)
            return null;
        const AnchorName = `--toolbar-menu-${++EditorToolbarMenu.NextAnchorId}`;
        if (typeof AnchorName !== "undefined")
            this.MenuAnchorName = AnchorName;
        this.TargetButton.style.setProperty("anchor-name", `--toolbar-tooltip, ${AnchorName}`);
        const ClonedTemplate = this.MenuTemplate.content.firstElementChild.cloneNode(true);
        this.ToolbarElement.appendChild(ClonedTemplate);
        this.MenuElement = ClonedTemplate;
        this.MenuElement.style.positionAnchor = "--toolbar";
        this.MenuElement.style.top = "calc(anchor(bottom) + 5.5px)";
        this.MenuElement.style.left = `anchor(${AnchorName} left)`;
        console.debug(`${this.TargetButton.name.toUpperCase()} MENU LEFT:`, this.MenuElement.style.left);
        return this.MenuElement;
    }
    initalize_position() {
    }
    toggle(target = undefined) {
        let conversion = { true: "flex", false: "none" };
        let key = new String(target).valueOf() || "false";
        let value = conversion[key];
    }
    constructor(TargetButton = null, ToolbarElement) {
        this.TargetButton = TargetButton;
        this.ToolbarElement = ToolbarElement;
        this.MenuTemplate = window.document.querySelector("#toolbar-item-menu");
        this.MenuElement = null;
        this.initialize_menu();
        console.debug(`INSTANCING NEW TOOLBAR MENU...`);
    }
    add_commands(name = "unknown") {
        const ButtonCommandEntries = Object.entries(EditorToolbarMenu.ToolbarButtonCommands);
        let CreatedCommandButtons = [];
        ButtonCommandEntries.forEach((CommandConfiguration) => {
            if (this.TargetButton != null && this.TargetButton?.name === CommandConfiguration[0]) {
                CommandConfiguration[1].forEach((config) => {
                    const CommandButton = document.createElement("button") ?? HTMLButtonElement.prototype;
                    const AppendedButton = this.MenuElement?.appendChild(CommandButton) || undefined;
                    if (AppendedButton == null) {
                        return;
                    }
                    AppendedButton.innerHTML = config.label.valueOf();
                    CreatedCommandButtons.push(AppendedButton);
                });
            }
        });
        return CreatedCommandButtons;
    }
}
