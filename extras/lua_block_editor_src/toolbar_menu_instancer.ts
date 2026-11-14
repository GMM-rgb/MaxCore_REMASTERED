export type ToolbarButtonCommand = {
    "label": string | String,
    "op": (() => void) | null,
};

export class EditorToolbarMenu {
    MenuElement: HTMLElement | Node | null;
    MenuTemplate: HTMLTemplateElement;

    static ToolbarButtonCommands: Record<string, Array<ToolbarButtonCommand>> = {
        "file": [
            {
                "label": "<strong>save project</strong>",
                "op": (): void => { },
            },
            {
                "label": "<strong>save as</strong>",
                "op": null,
            },
            {
                "label": "<strong>load project</strong>",
                "op": null,
            },
            {
                "label": "<strong>exit</strong>",
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
                default: console.warn("unkown button name");
            }
        }
    }

    public construct_menu(): HTMLElement | Node | null {
        const ClonedTemplate = this.MenuTemplate.content.firstElementChild!.cloneNode(true);

        this.ToolbarElement.appendChild(ClonedTemplate);
        this.MenuElement = ClonedTemplate as HTMLDivElement;

        console.debug(this.MenuElement, this.MenuElement instanceof HTMLElement);

        (this.MenuElement as HTMLElement)?.addEventListener("change", (e) => {
            console.info("`MenuElement` Changed:\t", e.target);
        });

        return this.MenuElement;
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
                });
            }
        });

        return CreatedCommandButtons;
    }
}
