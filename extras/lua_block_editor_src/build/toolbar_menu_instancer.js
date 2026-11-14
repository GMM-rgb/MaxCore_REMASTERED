let ToolbarMenus = [];
export class EditorToolbarMenu {
    TargetButton;
    ToolbarElement;
    MenuElement;
    MenuTemplate;
    static NextAnchorId = 0;
    MenuAnchorName = null;
    MenuTriggered = false;
    CommandButtons = [];
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
                "op": () => { },
            },
        ],
        "edit": [
            {
                "label": "<strong>Editor Theme</strong>",
                "op": null,
            },
            {
                "label": "",
                "op": null,
            }
        ],
    };
    check_others() {
        for (let ti = 0; ti < ToolbarMenus.length; ++ti) {
            if (ToolbarMenus[ti] !== this && ToolbarMenus[ti].MenuTriggered) {
                ToolbarMenus[ti].toggle(false);
            }
        }
    }
    apply_listener() {
        this?.TargetButton?.addEventListener("click", (e) => {
            this.toggle(!this.MenuTriggered);
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
                case "block-palette":
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
        this.MenuAnchorName = AnchorName;
        this.TargetButton.style.setProperty("anchor-name", `--toolbar-tooltip, ${AnchorName}`);
        const ClonedTemplate = this.MenuTemplate.content.firstElementChild.cloneNode(true);
        this.ToolbarElement.appendChild(ClonedTemplate);
        this.MenuElement = ClonedTemplate;
        this.MenuElement.style.top = "calc(anchor(bottom) + 5.5px)";
        this.MenuElement.style.left = `anchor(${AnchorName} left)`;
        const ButtonRect = this.TargetButton.getBoundingClientRect();
        const MenuRect = this.MenuElement.getBoundingClientRect();
        console.debug(`${this.TargetButton.name.toUpperCase()} MENU LEFT:`, this.MenuElement.style.left);
        console.debug(`VALID:\t%c${this.TargetButton instanceof HTMLElement}`, "color: violet;");
        this.MenuElement.style.setProperty("--menu-entrance-offset", `${ButtonRect.top - MenuRect.top}px`);
        const MenuName = String(this.TargetButton.name);
        this.TargetButton.setAttribute("data-menu", MenuName);
        this.MenuElement.setAttribute("data-menu", MenuName);
        return this.MenuElement;
    }
    initalize_position() {
    }
    toggle(target = undefined) {
        const tsn = "menu-focus-active";
        if (this.MenuElement == undefined)
            return;
        const ClassList = this.MenuElement.classList;
        const conversion_values = [
            [() => { return !ClassList.contains(tsn); }, ClassList.add.bind(ClassList)],
            [() => { return ClassList.contains(tsn); }, ClassList.remove.bind(ClassList)],
        ];
        const conversion = {
            true: [conversion_values[0][1], conversion_values[0][0]],
            false: [conversion_values[1][1], conversion_values[1][0]],
        };
        const value = conversion[String(target ?? false)];
        if (target === true)
            this.check_others();
        if (value[1]())
            value[0](tsn);
        this.MenuTriggered = target ?? false;
        this.TargetButton?.classList.toggle("menu-tooltip-hidden", this.MenuTriggered);
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
                    AppendedButton.addEventListener("click", () => { config.op?.(); });
                    CreatedCommandButtons.push(AppendedButton);
                });
            }
        });
        this.CommandButtons ??= CreatedCommandButtons;
        return CreatedCommandButtons;
    }
    constructor(TargetButton = null, ToolbarElement) {
        this.TargetButton = TargetButton;
        this.ToolbarElement = ToolbarElement;
        this.MenuTemplate = window.document.querySelector("#toolbar-item-menu");
        this.MenuElement = null;
        this.initialize_menu();
        ToolbarMenus.push(this);
        console.debug(`INSTANCING NEW TOOLBAR MENU...`);
    }
}
