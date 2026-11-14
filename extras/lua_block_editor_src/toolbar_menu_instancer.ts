export type ToolbarButtonCommand = {
    "label": string | String,
    "op": (() => void) | null,
};

let ToolbarMenus: EditorToolbarMenu[] = [];

export class EditorToolbarMenu {
    MenuElement: HTMLElement | null;
    MenuTemplate: HTMLTemplateElement;

    private static NextAnchorId: number = 0;
    private MenuAnchorName: string | null = null;
    private MenuTriggered: boolean = false;

    private static ToolbarButtonCommands: Record<string, Array<ToolbarButtonCommand>> = {
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
        "edit": [
            {
                "label": "<strong>Editor Theme</strong>",
                "op": null,
            }
        ],
    };

    private check_others(): void {
        for (let ti = 0; ti < ToolbarMenus.length; ++ti) {
            if (ToolbarMenus[ti] !== this && ToolbarMenus[ti].MenuTriggered) {
                ToolbarMenus[ti].toggle(false);
            }
        }
    }

    private apply_listener(): void {
        this?.TargetButton?.addEventListener("click", (e: PointerEvent): void => {
            // console.debug("CLICKED TOOLBAR BUTTON:", this.TargetButton?.name ?? undefined);
            this.toggle(!this.MenuTriggered);
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

    // public construct_menu(): HTMLElement | Node | null {
    //     if (!this.TargetButton) return null;

    //     const AnchorName = `--toolbar-menu-${++EditorToolbarMenu.NextAnchorId}`;
    //     if (typeof AnchorName !== "undefined") this.MenuAnchorName = AnchorName;

    //     // preserve the tooltip anchor while adding this menu's unique anchor name.
    //     this.TargetButton.style.setProperty("anchor-name", `--toolbar-tooltip, ${AnchorName}`);

    //     const ClonedTemplate = this.MenuTemplate.content.firstElementChild!.cloneNode(true);
    //     this.ToolbarElement.appendChild(ClonedTemplate);
    //     this.MenuElement = ClonedTemplate as HTMLDivElement;
    //     this.MenuElement.style.top = "calc(anchor(bottom) + 5.5px)";
    //     this.MenuElement.style.left = `anchor(${AnchorName} left)`;

    //     const ButtonRect = this.TargetButton.getBoundingClientRect();
    //     const MenuRect = this.MenuElement.getBoundingClientRect();

    //     this.MenuElement.style.setProperty("--menu-entrance-offset", `${ButtonRect.top - MenuRect.top}px`);
    //     console.debug(`${this.TargetButton.name.toUpperCase()} MENU LEFT:`, this.MenuElement.style.left);
    //     console.debug(`VALID:\t%c${this.TargetButton instanceof HTMLElement}`, "color: violet;");
    //     this.MenuElement.setAttribute("data-menu", String(this.TargetButton.name).trim());
    //     this.TargetButton.setAttribute("data-menu", String(this.TargetButton.name).trim());

    //     return this.MenuElement;
    // }

    public construct_menu(): HTMLElement | Node | null {
        if (!this.TargetButton) return null;

        const AnchorName = `--toolbar-menu-${++EditorToolbarMenu.NextAnchorId}`;
        this.MenuAnchorName = AnchorName;

        // Preserve the tooltip anchor while adding the menu's unique anchor.
        this.TargetButton.style.setProperty("anchor-name", `--toolbar-tooltip, ${AnchorName}`);

        const ClonedTemplate = this.MenuTemplate.content.firstElementChild!.cloneNode(true);
        this.ToolbarElement.appendChild(ClonedTemplate);
        this.MenuElement = ClonedTemplate as HTMLDivElement;
        this.MenuElement.style.top = "calc(anchor(bottom) + 5.5px)";
        this.MenuElement.style.left = `anchor(${AnchorName} left)`;

        const ButtonRect = this.TargetButton.getBoundingClientRect();
        const MenuRect = this.MenuElement.getBoundingClientRect();

        console.debug(`${this.TargetButton.name.toUpperCase()} MENU LEFT:`, this.MenuElement.style.left);
        console.debug(`VALID:\t%c${this.TargetButton instanceof HTMLElement}`, "color: violet;");

        this.MenuElement.style.setProperty("--menu-entrance-offset", `${ButtonRect.top - MenuRect.top}px`);

        // Associate the menu and its corresponding button.
        const MenuName = String(this.TargetButton.name);

        this.TargetButton.setAttribute("data-menu", MenuName);
        this.MenuElement.setAttribute("data-menu", MenuName);

        return this.MenuElement;
    }

    public initalize_position(): void {

    }

    public toggle(target: boolean | undefined = undefined) {
        type ClassListFunction = (...tokens: string[]) => void;
        type ConditionFunction = () => boolean;

        const tsn = "menu-focus-active"; // target class name toggle; removed / added
        if (this.MenuElement == undefined) return; const ClassList = this.MenuElement.classList;

        const conversion_values: [ConditionFunction, ClassListFunction][] = [
            [(): boolean => { return !ClassList.contains(tsn); }, ClassList.add.bind(ClassList)],
            [(): boolean => { return ClassList.contains(tsn); }, ClassList.remove.bind(ClassList)],
        ];

        const conversion: Record<string, [ClassListFunction, ConditionFunction]> = {
            true: [conversion_values[0][1], conversion_values[0][0]],
            false: [conversion_values[1][1], conversion_values[1][0]],
        };

        const value = conversion[String(target ?? false)];
    
        if (target === true) this.check_others();
        if (value[1]()) value[0](tsn);

        this.MenuTriggered = target ?? false;
        this.TargetButton?.classList.toggle("menu-tooltip-hidden", this.MenuTriggered);
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

    public constructor(
        private TargetButton: HTMLButtonElement | null = null,
        private ToolbarElement: HTMLDivElement
    ) {
        this.MenuTemplate = window.document.querySelector("#toolbar-item-menu") as HTMLTemplateElement;
        this.MenuElement = null; this.initialize_menu(); ToolbarMenus.push(this);
        console.debug(`INSTANCING NEW TOOLBAR MENU...`);
    }
}
