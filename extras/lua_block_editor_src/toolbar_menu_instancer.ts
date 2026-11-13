export class EditorToolbarMenu {
    MenuElement: HTMLElement | null;

    private apply_listener(): void {

    }

    private initialize_menu(): void {
        switch (this.TargetButton.name) {
            case "file": this.apply_listener(); break;
            case "edit": this.apply_listener(); break;
            default: console.warn("unkown button name");
        }
    }

    public constructor(
        private TargetButton: HTMLButtonElement
    ) {
        this.MenuElement = null;
        this.initialize_menu();
    }

    public add_commands(name: string = "unknown"): Array<HTMLButtonElement> {
        return [];
    }
}
