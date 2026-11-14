export type ToolbarButtonCommand = {
    "label": string | String,
    "op": (() => void) | null,
};

export class EditorToolbarMenu {
    MenuElement: HTMLElement | null;
    MenuTemplate: HTMLTemplateElement;

    private static NextAnchorId: number = 0;
    private MenuAnchorName: string | null = null;

    static ToolbarButtonCommands: Record<string, Array<ToolbarButtonCommand>> = {
        "file": [
            {
                "label": "<strong>Save Project</strong>",
                "op": (): void => { },
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
                "op": (): void => { window.close(); },
            },
        ],
    };

    private apply_listener(): void {
        this?.TargetButton?.addEventListener("click", (e: PointerEvent) => {
            console.debug("CLICKED TOOLBAR BUTTON:", this.TargetButton?.name ?? undefined);
        }, { passive: true });
    }

    private initialize_menu(): void {
        if (this.TargetButton) {
            switch (this.TargetButton.name) {
                case "file": this.apply_listener(); break;
                case "edit": this.apply_listener(); break;
                case "settings": this.apply_listener(); break;
                default: console.warn("unkown button name"); return;
            }
        }
    }

    public construct_menu(): HTMLElement | Node | null {
        if (!this.TargetButton) return null;

        const AnchorName = `--toolbar-menu-${++EditorToolbarMenu.NextAnchorId}`;
        if (typeof AnchorName !== "undefined") this.MenuAnchorName = AnchorName;

        // preserve the tooltip anchor while adding this menu's unique anchor name.
        this.TargetButton.style.setProperty("anchor-name", `--toolbar-tooltip, ${AnchorName}`);

        const ClonedTemplate = this.MenuTemplate.content.firstElementChild!.cloneNode(true);
        this.ToolbarElement.appendChild(ClonedTemplate);
        this.MenuElement = ClonedTemplate as HTMLDivElement;
        this.MenuElement.style.positionAnchor = "--toolbar";
        this.MenuElement.style.top = "calc(anchor(bottom) + 5.5px)";
        this.MenuElement.style.left = `anchor(${AnchorName} left)`;

        console.debug(`${this.TargetButton.name.toUpperCase()} MENU LEFT:`, this.MenuElement.style.left);

        return this.MenuElement;
    }

    public initalize_position(): void {

    }

    public toggle(target: boolean | undefined = undefined) {
        let conversion: Record<string, string> = { true: "flex", false: "none" };
        let key = new String(target).valueOf() || "false";
        let value = conversion[key];
    }

    public constructor(
        private TargetButton: HTMLButtonElement | null = null,
        private ToolbarElement: HTMLDivElement
    ) {
        this.MenuTemplate = window.document.querySelector("#toolbar-item-menu") as HTMLTemplateElement;
        this.MenuElement = null; this.initialize_menu(); console.debug(`INSTANCING NEW TOOLBAR MENU...`);
    }

    public add_commands<T>(name: string = "unknown"): T[] {
        const ButtonCommandEntries = Object.entries(EditorToolbarMenu.ToolbarButtonCommands);
        let CreatedCommandButtons: T[] = [];

        ButtonCommandEntries.forEach((CommandConfiguration: [string, ToolbarButtonCommand[]]): void => {
            if (this.TargetButton != null && this.TargetButton?.name === CommandConfiguration[0]) {                
                CommandConfiguration[1].forEach((config: ToolbarButtonCommand) => {
                    const CommandButton = document.createElement("button") ?? HTMLButtonElement.prototype;
                    const AppendedButton = this.MenuElement?.appendChild(CommandButton) || undefined;
                    if (AppendedButton == null) { return; } AppendedButton.innerHTML = config.label.valueOf();
                    CreatedCommandButtons.push(AppendedButton as T);
                });
            }
        });

        return CreatedCommandButtons;
    }
}
