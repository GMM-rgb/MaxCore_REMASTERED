var EditorToolbarMenu = (function () {
    function EditorToolbarMenu(TargetButton, ToolbarElement) {
        if (TargetButton === void 0) { TargetButton = null; }
        this.TargetButton = TargetButton;
        this.ToolbarElement = ToolbarElement;
        this.MenuTemplate = window.document.querySelector("#toolbar-item-menu");
        this.MenuElement = null;
        this.initialize_menu();
        console.debug("INSTANCING NEW TOOLBAR MENU...");
    }
    EditorToolbarMenu.prototype.apply_listener = function () {
        var _this = this;
        var _a;
        (_a = this === null || this === void 0 ? void 0 : this.TargetButton) === null || _a === void 0 ? void 0 : _a.addEventListener("click", function (e) {
            var _a, _b;
            console.debug("CLICKED TOOLBAR BUTTON:", (_b = (_a = _this.TargetButton) === null || _a === void 0 ? void 0 : _a.name) !== null && _b !== void 0 ? _b : undefined);
        }, { passive: true });
    };
    EditorToolbarMenu.prototype.initialize_menu = function () {
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
                default: console.warn("unkown button name");
            }
        }
    };
    EditorToolbarMenu.prototype.construct_menu = function () {
        var _a;
        var ClonedTemplate = this.MenuTemplate.content.firstElementChild.cloneNode(true);
        this.ToolbarElement.appendChild(ClonedTemplate);
        this.MenuElement = ClonedTemplate;
        console.debug(this.MenuElement, this.MenuElement instanceof HTMLElement);
        (_a = this.MenuElement) === null || _a === void 0 ? void 0 : _a.addEventListener("change", function (e) {
            console.info("`MenuElement` Changed:\t", e.target);
        });
        return this.MenuElement;
    };
    EditorToolbarMenu.prototype.toggle = function (target) {
        if (target === void 0) { target = undefined; }
        var conversion = { true: "flex", false: "none" };
        var key = new String(target).valueOf() || "false";
        var value = conversion[key];
    };
    EditorToolbarMenu.prototype.add_commands = function (name) {
        var _this = this;
        if (name === void 0) { name = "unknown"; }
        var ButtonCommandEntries = Object.entries(EditorToolbarMenu.ToolbarButtonCommands);
        var CreatedCommandButtons = [];
        ButtonCommandEntries.forEach(function (CommandConfiguration) {
            var _a;
            if (_this.TargetButton != null && ((_a = _this.TargetButton) === null || _a === void 0 ? void 0 : _a.name) === CommandConfiguration[0]) {
                CommandConfiguration[1].forEach(function (config) {
                    var _a, _b;
                    var CommandButton = (_a = document.createElement("button")) !== null && _a !== void 0 ? _a : HTMLButtonElement.prototype;
                    var AppendedButton = ((_b = _this.MenuElement) === null || _b === void 0 ? void 0 : _b.appendChild(CommandButton)) || undefined;
                    if (AppendedButton == null) {
                        return;
                    }
                    AppendedButton.innerHTML = config.label.valueOf();
                });
            }
        });
        return CreatedCommandButtons;
    };
    EditorToolbarMenu.ToolbarButtonCommands = {
        "file": [
            {
                "label": "<strong>save project</strong>",
                "op": function () { },
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
                "op": function () { window.close(); },
            },
        ],
    };
    return EditorToolbarMenu;
}());
export { EditorToolbarMenu };
