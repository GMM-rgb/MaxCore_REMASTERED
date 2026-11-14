var ToolbarMenus = [];
var EditorToolbarMenu = (function () {
    function EditorToolbarMenu(TargetButton, ToolbarElement) {
        if (TargetButton === void 0) { TargetButton = null; }
        this.TargetButton = TargetButton;
        this.ToolbarElement = ToolbarElement;
        this.MenuAnchorName = null;
        this.MenuTriggered = false;
        this.MenuTemplate = window.document.querySelector("#toolbar-item-menu");
        this.MenuElement = null;
        this.initialize_menu();
        ToolbarMenus.push(this);
        console.debug("INSTANCING NEW TOOLBAR MENU...");
    }
    EditorToolbarMenu.prototype.check_others = function () {
        for (var ti = 0; ti < ToolbarMenus.length; ++ti) {
            if (ToolbarMenus[ti] !== this && ToolbarMenus[ti].MenuTriggered) {
                ToolbarMenus[ti].toggle(false);
            }
        }
    };
    EditorToolbarMenu.prototype.apply_listener = function () {
        var _this = this;
        var _a;
        (_a = this === null || this === void 0 ? void 0 : this.TargetButton) === null || _a === void 0 ? void 0 : _a.addEventListener("click", function (e) {
            _this.toggle(!_this.MenuTriggered);
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
                default:
                    console.warn("unkown button name");
                    return;
            }
        }
    };
    EditorToolbarMenu.prototype.construct_menu = function () {
        if (!this.TargetButton)
            return null;
        var AnchorName = "--toolbar-menu-".concat(++EditorToolbarMenu.NextAnchorId);
        this.MenuAnchorName = AnchorName;
        this.TargetButton.style.setProperty("anchor-name", "--toolbar-tooltip, ".concat(AnchorName));
        var ClonedTemplate = this.MenuTemplate.content.firstElementChild.cloneNode(true);
        this.ToolbarElement.appendChild(ClonedTemplate);
        this.MenuElement = ClonedTemplate;
        this.MenuElement.style.top = "calc(anchor(bottom) + 5.5px)";
        this.MenuElement.style.left = "anchor(".concat(AnchorName, " left)");
        var ButtonRect = this.TargetButton.getBoundingClientRect();
        var MenuRect = this.MenuElement.getBoundingClientRect();
        console.debug("".concat(this.TargetButton.name.toUpperCase(), " MENU LEFT:"), this.MenuElement.style.left);
        console.debug("VALID:\t%c".concat(this.TargetButton instanceof HTMLElement), "color: violet;");
        this.MenuElement.style.setProperty("--menu-entrance-offset", "".concat(ButtonRect.top - MenuRect.top, "px"));
        var MenuName = String(this.TargetButton.name);
        this.TargetButton.setAttribute("data-menu", MenuName);
        this.MenuElement.setAttribute("data-menu", MenuName);
        return this.MenuElement;
    };
    EditorToolbarMenu.prototype.initalize_position = function () {
    };
    EditorToolbarMenu.prototype.toggle = function (target) {
        var _a;
        if (target === void 0) { target = undefined; }
        var tsn = "menu-focus-active";
        if (this.MenuElement == undefined)
            return;
        var ClassList = this.MenuElement.classList;
        var conversion_values = [
            [function () { return !ClassList.contains(tsn); }, ClassList.add.bind(ClassList)],
            [function () { return ClassList.contains(tsn); }, ClassList.remove.bind(ClassList)],
        ];
        var conversion = {
            true: [conversion_values[0][1], conversion_values[0][0]],
            false: [conversion_values[1][1], conversion_values[1][0]],
        };
        var value = conversion[String(target !== null && target !== void 0 ? target : false)];
        if (target === true)
            this.check_others();
        if (value[1]())
            value[0](tsn);
        this.MenuTriggered = target !== null && target !== void 0 ? target : false;
        (_a = this.TargetButton) === null || _a === void 0 ? void 0 : _a.classList.toggle("menu-tooltip-hidden", this.MenuTriggered);
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
                    CreatedCommandButtons.push(AppendedButton);
                });
            }
        });
        return CreatedCommandButtons;
    };
    EditorToolbarMenu.NextAnchorId = 0;
    EditorToolbarMenu.ToolbarButtonCommands = {
        "file": [
            {
                "label": "<strong>Save Project</strong>",
                "op": function () { },
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
                "op": function () { window.close(); },
            },
        ],
        "edit": [
            {
                "label": "<strong>Editor Theme</strong>",
                "op": null,
            }
        ],
    };
    return EditorToolbarMenu;
}());
export { EditorToolbarMenu };
