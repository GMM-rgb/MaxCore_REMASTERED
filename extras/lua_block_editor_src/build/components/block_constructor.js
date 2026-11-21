import { VirtualMachine, VirtualMachineParser } from './blbvm.js';
export var BlockBuilderMachine;
(function (BlockBuilderMachine) {
    BlockBuilderMachine.VM = new VirtualMachine.Machine({});
    BlockBuilderMachine.CreatedBlocks = [];
    let CommandBlocks;
    (function (CommandBlocks) {
        CommandBlocks[CommandBlocks["COM"] = 0] = "COM";
        CommandBlocks[CommandBlocks["HAT"] = 1] = "HAT";
        CommandBlocks[CommandBlocks["END"] = 2] = "END";
        CommandBlocks[CommandBlocks["REP"] = 3] = "REP";
    })(CommandBlocks = BlockBuilderMachine.CommandBlocks || (BlockBuilderMachine.CommandBlocks = {}));
    let OutputBlocks;
    (function (OutputBlocks) {
        OutputBlocks[OutputBlocks["STRING"] = 4] = "STRING";
        OutputBlocks[OutputBlocks["BOOL"] = 5] = "BOOL";
    })(OutputBlocks = BlockBuilderMachine.OutputBlocks || (BlockBuilderMachine.OutputBlocks = {}));
    let InputTypes;
    (function (InputTypes) {
        InputTypes[InputTypes["STRING"] = 0] = "STRING";
        InputTypes[InputTypes["NUMBER"] = 1] = "NUMBER";
        InputTypes[InputTypes["BOOL"] = 2] = "BOOL";
        InputTypes[InputTypes["VARIABLE"] = 3] = "VARIABLE";
        InputTypes[InputTypes["BLOCK"] = 4] = "BLOCK";
    })(InputTypes = BlockBuilderMachine.InputTypes || (BlockBuilderMachine.InputTypes = {}));
    class Block {
        params;
        type;
        lua_op;
        connecting;
        block_notches;
        connections;
        element;
        inputs;
        label;
        id;
        parent_connection = null;
        input_connections = new Map();
        parent_input = null;
        is_toolbox_clone = false;
        embed_radius = null;
        measurement_svg = null;
        measurement_text = null;
        drag_preview_layer = null;
        namespace = "http://www.w3.org/2000/svg";
        default_layout = {
            padding: { x: 12, y: 8 },
            spacing: 4,
            input_spacing: 8,
            alignment: "left",
        };
        constructor(params, type, text) {
            this.params = params;
            this.type = type;
            const { lua_code } = params;
            this.element = null;
            this.connecting = false;
            this.label = text.trim();
            this.lua_op = (lua_code ?? "").trim();
            this.id = this.initialize_unique();
            this.connections = [];
            this.block_notches = [];
            this.inputs = [];
        }
        get default_input_style() {
            return {
                fill: "#e8ebfa",
                stroke: "#303f80",
                stroke_width: 1,
                radius: 5,
                padding: { x: 8, y: 4 },
                font: this.params.font,
                text_fill: "#20294f",
            };
        }
        initialize_unique() {
            return crypto.randomUUID();
        }
        copy_config(template = false) {
            const config = this.params;
            return {
                ...config,
                template,
                geometry: config.geometry ? {
                    ...config.geometry,
                    notch: { ...config.geometry.notch },
                } : undefined,
                input_style: config.input_style ? {
                    ...config.input_style,
                    padding: { ...config.input_style.padding },
                } : undefined,
                layout: config.layout ? {
                    ...config.layout,
                    padding: { ...config.layout.padding },
                } : undefined,
            };
        }
        clone_current_contents() {
            const clone = new Block(this.copy_config(false), this.type, this.label);
            clone.is_toolbox_clone = true;
            clone.inputs = this.inputs.map((input) => ({ ...input }));
            for (const [index, child] of this.input_connections) {
                const childClone = child.clone_current_contents();
                clone.input_connections.set(index, childClone);
                childClone.parent_input = { block: clone, index };
            }
            clone.invoke();
            return clone;
        }
        get is_template() {
            return this.params.template === true;
        }
        get layout() {
            return this.params.layout ?? this.default_layout;
        }
        get input_style() {
            return this.params.input_style ?? this.default_input_style;
        }
        get_measurement_elements() {
            if (!this.measurement_svg || !this.measurement_text) {
                const svg = document.createElementNS(this.namespace, "svg");
                const text = document.createElementNS(this.namespace, "text");
                svg.style.position = "absolute";
                svg.style.visibility = "hidden";
                svg.style.pointerEvents = "none";
                svg.style.overflow = "visible";
                svg.appendChild(text);
                document.body.appendChild(svg);
                this.measurement_svg = svg;
                this.measurement_text = text;
            }
            return this.measurement_text;
        }
        get_font_size(font) {
            return Number(font.match(/(\d+(?:\.\d+)?)px/)?.[1] ?? 12);
        }
        apply_segment_style(element, style) {
            if (style.bold)
                element.style.fontWeight = "bold";
            if (style.italic)
                element.style.fontStyle = "italic";
            if (style.decoration)
                element.style.textDecoration = style.decoration;
            if (style.font_family)
                element.style.fontFamily = style.font_family;
        }
        get_text_segments(source) {
            const parsed = new DOMParser().parseFromString(source, "text/html");
            const segments = [];
            const append_nodes = (parent, inherited) => {
                for (const node of Array.from(parent.childNodes)) {
                    if (node.nodeType === Node.TEXT_NODE) {
                        const content = node.textContent ?? "";
                        if (content.length > 0) {
                            segments.push({ content, style: { ...inherited } });
                        }
                        continue;
                    }
                    if (node.nodeType !== Node.ELEMENT_NODE)
                        continue;
                    const element = node;
                    const style = { ...inherited };
                    switch (element.tagName.toLowerCase()) {
                        case "b":
                        case "strong":
                            style.bold = true;
                            break;
                        case "i":
                        case "em":
                            style.italic = true;
                            break;
                        case "u":
                            style.decoration = "underline";
                            break;
                        case "s":
                        case "del":
                            style.decoration = "line-through";
                            break;
                        case "code":
                            style.font_family = "monospace";
                            break;
                    }
                    append_nodes(element, style);
                }
            };
            append_nodes(parsed.body, {
                bold: false,
                italic: false,
                decoration: "",
                font_family: null,
            });
            return segments;
        }
        measure_segment(content, style) {
            const text = this.get_measurement_elements();
            text.replaceChildren();
            text.style.font = this.params.font;
            const tspan = document.createElementNS(this.namespace, "tspan");
            this.apply_segment_style(tspan, style);
            tspan.textContent = content;
            text.appendChild(tspan);
            return text.getComputedTextLength();
        }
        measure_text(content, font = this.params.font) {
            const text = this.get_measurement_elements();
            text.replaceChildren();
            text.style.font = font;
            const tspan = document.createElementNS(this.namespace, "tspan");
            tspan.textContent = content;
            text.appendChild(tspan);
            return text.getComputedTextLength();
        }
        get_segment_width(segment) {
            const style = this.input_style;
            return this.measure_segment(segment.content, segment.style) + style.padding.x * 2;
        }
        get_input_display_value(input) {
            return String(input.content);
        }
        validate_input(input) {
            if (input.width != null && (!Number.isFinite(input.width) || input.width <= 0)) {
                throw new RangeError("Block parameter width must be a positive finite number.");
            }
            switch (input.type) {
                case InputTypes.STRING:
                case InputTypes.VARIABLE:
                case InputTypes.BLOCK:
                    if (typeof input.content !== "string") {
                        throw new TypeError("String, variable, and block inputs require string content.");
                    }
                    break;
                case InputTypes.NUMBER:
                    if (typeof input.content !== "number" || !Number.isFinite(input.content)) {
                        throw new TypeError("Number inputs require a finite numeric value.");
                    }
                    break;
                case InputTypes.BOOL:
                    if (typeof input.content !== "boolean") {
                        throw new TypeError("Boolean inputs require boolean content.");
                    }
                    break;
                default:
                    throw new TypeError("Invalid block input type.");
            }
        }
        capture_visual_state(element) {
            return {
                position: element.style.position,
                left: element.style.left,
                top: element.style.top,
                right: element.style.right,
                bottom: element.style.bottom,
                zIndex: element.style.zIndex,
                transform: element.style.transform,
                display: element.style.display,
                flex: element.style.flex,
                margin: element.style.margin,
                cursor: element.style.cursor,
            };
        }
        apply_visual_state(element, state) {
            element.style.position = state.position;
            element.style.left = state.left;
            element.style.top = state.top;
            element.style.right = state.right;
            element.style.bottom = state.bottom;
            element.style.zIndex = state.zIndex;
            element.style.transform = state.transform;
            element.style.display = state.display;
            element.style.flex = state.flex;
            element.style.margin = state.margin;
            element.style.cursor = state.cursor;
        }
        refresh_input_layout(input, raw_value) {
            const previous = this.element;
            if (!previous)
                return;
            const parent = previous.parentNode;
            if (!parent)
                return;
            const index = input.dataset.inputIndex;
            const selection_start = input.selectionStart;
            const selection_end = input.selectionEnd;
            const selection_direction = input.selectionDirection;
            const had_focus = document.activeElement === input;
            const restore_selection = had_focus && selection_start !== null &&
                selection_end !== null && input.type !== "checkbox" && input.type !== "number";
            const state = this.capture_visual_state(previous);
            this.element = null;
            const updated = this.invoke();
            this.apply_visual_state(updated, state);
            parent.replaceChild(updated, previous);
            const replacement = this.find_input_element(index);
            if (replacement && raw_value !== undefined && replacement.type !== "checkbox") {
                replacement.value = raw_value;
            }
            this.parent_input?.block.refresh_render_preserving_position();
            if (!had_focus)
                return;
            const restored = this.find_input_element(index);
            if (!restored)
                return;
            if (raw_value !== undefined && restored.type !== "checkbox") {
                restored.value = raw_value;
            }
            restored.focus();
            if (restore_selection && selection_start !== null && selection_end !== null) {
                try {
                    restored.setSelectionRange(selection_start, selection_end, selection_direction ?? undefined);
                }
                catch {
                }
            }
        }
        set_embed_radius(radius) {
            if (this.embed_radius === radius)
                return;
            this.embed_radius = radius;
            this.rerender_in_place();
        }
        rerender_in_place() {
            const previous = this.element;
            if (!previous)
                return;
            const parent = previous.parentNode;
            const state = this.capture_visual_state(previous);
            this.element = null;
            const updated = this.invoke();
            this.apply_visual_state(updated, state);
            if (parent) {
                parent.replaceChild(updated, previous);
            }
            else {
                previous.replaceWith(updated);
            }
        }
        render_detached(radius = null) {
            const live = this.element;
            const previous_radius = this.embed_radius;
            this.element = null;
            this.embed_radius = radius;
            const element = this.invoke();
            this.embed_radius = previous_radius;
            this.element = live;
            return element;
        }
        find_input_element(index) {
            const element = this.element;
            if (!element || index === undefined)
                return null;
            return element.querySelector(`input[data-block-id="${this.id}"][data-input-index="${index}"]`);
        }
        calculate_stroke_color(fill) {
            const color = fill.match(/^#([0-9a-f]{6})$/i);
            if (!color)
                return fill;
            const hex = color[1];
            const channels = [0, 2, 4].map(index => parseInt(hex.substring(index, index + 2), 16));
            const darker = channels.map(channel => Math.round(channel * 0.7));
            return `#${darker.map(channel => channel.toString(16).padStart(2, "0")).join("")}`;
        }
        allocate_input_id() {
            const used = new Set(this.inputs.map((existing, index) => existing.input_id ?? index));
            let candidate = 0;
            while (used.has(candidate))
                candidate++;
            return candidate;
        }
        add_param(input) {
            this.validate_input(input);
            const input_id = input.input_id ?? this.allocate_input_id();
            if (!Number.isSafeInteger(input_id) || input_id < 0) {
                throw new RangeError("Input ID must be a non-negative safe integer.");
            }
            const duplicate = this.inputs.some((existing, index) => (existing.input_id ?? index) === input_id);
            if (duplicate) {
                throw new Error(`Input ID '${input_id}' is already registered on block '${this.label}'.`);
            }
            this.inputs.push({ ...input, input_id });
            if (this.element) {
                const previous = this.element;
                const parent = previous.parentNode;
                const state = this.capture_visual_state(previous);
                this.element = null;
                const updated = this.invoke();
                this.apply_visual_state(updated, state);
                if (parent) {
                    parent.replaceChild(updated, previous);
                }
                else {
                    previous.remove();
                }
                this.parent_input?.block.refresh_render_preserving_position();
            }
            return this;
        }
        get_input_width(input, index) {
            const connected = this.input_connections.get(index);
            if (connected) {
                return Math.max(input.width ?? 0, connected.get_render_size().width);
            }
            const style = this.input_style;
            const font_size = this.get_font_size(style.font);
            let contentWidth;
            if (input.type === InputTypes.BOOL) {
                contentWidth = font_size;
            }
            else if (input.type === InputTypes.BLOCK) {
                contentWidth = this.measure_text(String(input.content || "..."), style.font);
            }
            else {
                contentWidth = this.measure_text(this.get_input_display_value(input), style.font);
            }
            const automaticWidth = contentWidth + style.padding.x * 2 + style.stroke_width * 2 + 2;
            return Math.max(input.width ?? 0, automaticWidth);
        }
        get_input_height(input, index) {
            const connected = this.input_connections.get(index);
            if (connected) {
                return connected.get_render_size().height;
            }
            const style = this.input_style;
            const font_size = this.get_font_size(style.font);
            if (input.type === InputTypes.BOOL) {
                return font_size + style.padding.y * 2;
            }
            return font_size + style.padding.y * 2;
        }
        get_render_size() {
            const size = this.calculate_block_size();
            const strokeWidth = this.params.stroke_width ?? 1.5;
            const geometry = this.get_geometry();
            const notch = this.get_notch_layout(size, strokeWidth);
            const extraHeight = geometry.bottom_connector && notch.enabled ? geometry.notch.depth : 0;
            return {
                width: size.width + strokeWidth,
                height: size.height + extraHeight + strokeWidth,
            };
        }
        get_row_items() {
            const items = [];
            for (const segment of this.get_text_segments(this.label)) {
                items.push({ kind: "label", segment });
            }
            for (let index = 0; index < this.inputs.length; index++) {
                items.push({ kind: "input", input: this.inputs[index], index });
            }
            return items;
        }
        get_row_item_width(item) {
            return item.kind === "label"
                ? this.get_segment_width(item.segment)
                : this.get_input_width(item.input, item.index);
        }
        get_row_item_height(item) {
            return item.kind === "label"
                ? this.get_font_size(this.params.font) + this.input_style.padding.y * 2
                : this.get_input_height(item.input, item.index);
        }
        get_row_gap(left, right) {
            const layout = this.layout;
            return left.kind === "input" || right.kind === "input"
                ? layout.input_spacing
                : layout.spacing;
        }
        measure_row(items) {
            let width = 0;
            for (let index = 0; index < items.length; index++) {
                width += this.get_row_item_width(items[index]);
                if (index > 0)
                    width += this.get_row_gap(items[index - 1], items[index]);
            }
            return width;
        }
        calculate_block_size() {
            const layout = this.layout;
            const style = this.input_style;
            const geometry = this.get_geometry();
            const items = this.get_row_items();
            const contentWidth = this.measure_row(items);
            const has_connectors = geometry.bottom_connector || geometry.top_notch;
            const notch_start = geometry.notch.offset ?? geometry.radius * 2;
            const minimumWidth = has_connectors
                ? notch_start + geometry.notch.width * 2 + geometry.radius
                : geometry.radius * 2;
            const minimumHeight = geometry.radius * 2 +
                (geometry.bottom_connector ? geometry.notch.depth : 0);
            const textHeight = this.get_font_size(this.params.font) + style.padding.y * 2;
            const rowHeight = Math.max(textHeight, items.reduce((maximum, item) => Math.max(maximum, this.get_row_item_height(item)), 0));
            return {
                width: Math.max(minimumWidth, contentWidth + layout.padding.x * 2),
                height: Math.max(minimumHeight, rowHeight + layout.padding.y * 2),
            };
        }
        get_notch_layout(size, strokeWidth) {
            const geometry = this.get_geometry();
            const radius = Math.min(geometry.radius, size.height / 2, size.width / 2);
            const notchRadius = Math.max(0, Math.min(radius, geometry.notch.depth / 2, geometry.notch.width / 2));
            const minimumStart = Math.max(radius * 2, radius + notchRadius, radius + strokeWidth);
            const start = Math.max(minimumStart, geometry.notch.offset ?? minimumStart);
            const end = Math.min(start + geometry.notch.width * 2, size.width - radius - notchRadius);
            return {
                start,
                end,
                radius: notchRadius,
                enabled: end - start > notchRadius * 2,
            };
        }
        construct_block_contents(size, strokeWidth) {
            const geometry = this.get_geometry();
            const { width, height } = size;
            const { depth } = geometry.notch;
            const outerRadius = Math.min(geometry.radius, height / 2, width / 2);
            const notch = this.get_notch_layout(size, strokeWidth);
            const commands = [];
            commands.push(`M 0 ${outerRadius}`);
            commands.push(`L 0 ${height - outerRadius}`);
            commands.push(`Q 0 ${height} ${outerRadius} ${height}`);
            this.block_notches = [];
            if (geometry.bottom_connector && notch.enabled) {
                const { start, end, radius } = notch;
                this.block_notches.push({
                    side: "bottom",
                    position: { x: (start + end) / 2, y: height },
                });
                commands.push(`L ${start - radius} ${height}`);
                commands.push(`Q ${start} ${height} ${start} ${height + radius}`);
                commands.push(`L ${start} ${height + depth - radius}`);
                commands.push(`Q ${start} ${height + depth} ${start + radius} ${height + depth}`);
                commands.push(`L ${end - radius} ${height + depth}`);
                commands.push(`Q ${end} ${height + depth} ${end} ${height + depth - radius}`);
                commands.push(`L ${end} ${height + radius}`);
                commands.push(`Q ${end} ${height} ${end + radius} ${height}`);
            }
            commands.push(`L ${width - outerRadius} ${height}`);
            commands.push(`Q ${width} ${height} ${width} ${height - outerRadius}`);
            commands.push(`L ${width} ${outerRadius}`);
            commands.push(`Q ${width} 0 ${width - outerRadius} 0`);
            if (geometry.top_notch && notch.enabled) {
                const { start, end, radius } = notch;
                this.block_notches.push({
                    side: "top",
                    position: { x: (start + end) / 2, y: 0 },
                });
                commands.push(`L ${end + radius} 0`);
                commands.push(`Q ${end} 0 ${end} ${radius}`);
                commands.push(`L ${end} ${depth - radius}`);
                commands.push(`Q ${end} ${depth} ${end - radius} ${depth}`);
                commands.push(`L ${start + radius} ${depth}`);
                commands.push(`Q ${start} ${depth} ${start} ${depth - radius}`);
                commands.push(`L ${start} ${radius}`);
                commands.push(`Q ${start} 0 ${start - radius} 0`);
            }
            commands.push(`L ${outerRadius} 0`);
            commands.push(`Q 0 0 0 ${outerRadius}`);
            commands.push("Z");
            return commands.join("\n");
        }
        create_input_element(input, x, y, index) {
            const style = this.input_style;
            const group = document.createElementNS(this.namespace, "g");
            const foreignObject = document.createElementNS(this.namespace, "foreignObject");
            const width = this.get_input_width(input, index);
            const height = this.get_input_height(input, index);
            const container = document.createElement("div");
            container.style.width = `${width}px`;
            container.style.height = `${height}px`;
            container.style.boxSizing = "border-box";
            container.style.display = "flex";
            container.style.alignItems = "center";
            container.style.justifyContent = "center";
            container.style.overflow = "visible";
            container.style.whiteSpace = "nowrap";
            container.className = "block-input-slot";
            container.dataset.blockId = this.id;
            container.dataset.blockInputIndex = String(index);
            container.dataset.inputType = InputTypes[input.type].toLowerCase();
            const connected = this.input_connections.get(index);
            if (connected) {
                connected.set_embed_radius(style.radius + 3);
            }
            const occupant = connected?.element ?? null;
            if (occupant) {
                container.style.border = "none";
                container.style.background = "transparent";
                container.style.padding = "0";
                occupant.style.position = "relative";
                occupant.style.left = "";
                occupant.style.top = "";
                occupant.style.right = "auto";
                occupant.style.bottom = "auto";
                occupant.style.zIndex = "";
                occupant.style.display = "block";
                occupant.style.flex = "none";
                occupant.style.margin = "0";
                occupant.style.cursor = "grab";
                container.appendChild(occupant);
                return this.assemble_input_group(group, foreignObject, container, input, x, y, width, height);
            }
            if (input.type === InputTypes.BLOCK) {
                container.style.border = `${style.stroke_width}px dashed ${style.stroke}`;
                container.style.borderRadius = `${style.radius + 3}px`;
                container.style.background = style.fill;
                container.style.color = style.text_fill;
                container.style.font = style.font;
                container.style.padding = `${style.padding.y}px ${style.padding.x}px`;
                container.textContent = String(input.content || "...");
                return this.assemble_input_group(group, foreignObject, container, input, x, y, width, height);
            }
            const element = document.createElement("input");
            element.setAttribute("name", "type-" + String(input.type));
            switch (input.type) {
                case InputTypes.NUMBER:
                    element.type = "number";
                    element.step = "any";
                    element.value = String(input.content);
                    break;
                case InputTypes.BOOL:
                    element.type = "checkbox";
                    element.checked = input.content;
                    break;
                default:
                    element.type = "text";
                    element.value = input.content;
                    break;
            }
            element.classList.add("block-input");
            element.dataset.inputType = InputTypes[input.type].toLowerCase();
            element.dataset.inputIndex = String(index);
            element.dataset.inputId = String(input.input_id ?? index);
            element.dataset.blockId = this.id;
            element.style.boxSizing = "border-box";
            element.style.border = `${style.stroke_width}px solid ${style.stroke}`;
            element.style.borderRadius = `${style.radius}px`;
            element.style.background = style.fill;
            element.style.color = style.text_fill;
            element.style.font = style.font;
            element.style.outline = "none";
            element.style.userSelect = "text";
            element.style.webkitUserSelect = "text";
            element.style.cursor = input.type === InputTypes.BOOL ? "pointer" : "text";
            if (input.type === InputTypes.BOOL) {
                element.style.width = "16px";
                element.style.height = "16px";
                element.style.margin = "0";
            }
            else {
                element.style.width = "100%";
                element.style.height = "100%";
                element.style.padding = `${style.padding.y}px ${style.padding.x}px`;
                element.style.minWidth = "0";
            }
            element.addEventListener("input", () => {
                if (input.type === InputTypes.NUMBER) {
                    if (element.value === "")
                        return;
                    const value = element.valueAsNumber;
                    if (Number.isFinite(value))
                        input.content = value;
                }
                else if (input.type !== InputTypes.BOOL) {
                    input.content = element.value;
                }
                const rendered_width = Number.parseFloat(element.parentElement?.style.width ?? "");
                const required_width = this.get_input_width(input, index);
                if (!(Number.isFinite(rendered_width) && Math.abs(rendered_width - required_width) < 0.5)) {
                    this.refresh_input_layout(element, element.value);
                }
                console.debug("Refreshing Block...", this);
                let VirtualBlock = BlockBuilderMachine.VM.get_block(this.id);
                if (!VirtualBlock)
                    VirtualBlock = BlockBuilderMachine.VM.inject_block(this);
                VirtualMachineParser.parse_generation(VirtualBlock);
                BlockWorkspaceEditor.refresh_content_bounds();
            });
            element.addEventListener("change", () => {
                if (input.type === InputTypes.BOOL) {
                    input.content = element.checked;
                }
                BlockWorkspaceEditor.refresh_content_bounds();
            });
            element.addEventListener("blur", () => {
                BlockWorkspaceEditor.refresh_content_bounds();
            });
            container.appendChild(element);
            return this.assemble_input_group(group, foreignObject, container, input, x, y, width, height);
        }
        assemble_input_group(group, foreignObject, container, input, x, y, width, height) {
            foreignObject.setAttribute("x", String(x));
            foreignObject.setAttribute("y", String(y));
            foreignObject.setAttribute("width", String(width));
            foreignObject.setAttribute("height", String(height));
            foreignObject.appendChild(container);
            group.appendChild(foreignObject);
            group.dataset.inputType = InputTypes[input.type].toLowerCase();
            group.dataset.inputId = String(input.input_id ?? this.inputs.indexOf(input));
            return group;
        }
        create_label_element(segment, x, y) {
            const style = this.input_style;
            const group = document.createElementNS(this.namespace, "g");
            const rect = document.createElementNS(this.namespace, "rect");
            const text = document.createElementNS(this.namespace, "text");
            const textWidth = this.measure_segment(segment.content, segment.style);
            const width = textWidth + style.padding.x * 2;
            const height = this.get_font_size(this.params.font) + style.padding.y * 2;
            rect.setAttribute("x", String(x));
            rect.setAttribute("y", String(y));
            rect.setAttribute("width", String(width));
            rect.setAttribute("height", String(height));
            rect.setAttribute("rx", String(style.radius));
            rect.setAttribute("fill", style.fill);
            rect.setAttribute("stroke", style.stroke);
            rect.setAttribute("stroke-width", String(style.stroke_width));
            text.setAttribute("x", String(x + style.padding.x));
            text.setAttribute("y", String(y + height / 2));
            text.setAttribute("dominant-baseline", "middle");
            text.setAttribute("fill", this.params.text_fill ?? "#111111");
            text.style.font = this.params.font;
            const tspan = document.createElementNS(this.namespace, "tspan");
            this.apply_segment_style(tspan, segment.style);
            tspan.textContent = segment.content;
            text.appendChild(tspan);
            group.appendChild(rect);
            group.appendChild(text);
            return group;
        }
        disable_text_selection() {
            const element = this.element;
            if (!element)
                return;
            element.style.userSelect = "none";
            element.style.webkitUserSelect = "none";
            element.addEventListener("focusin", (event) => {
                const target = event.target;
                if (target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement) {
                    target.style.userSelect = "text";
                    target.style.webkitUserSelect = "text";
                }
            });
            element.addEventListener("focusout", (event) => {
                const target = event.target;
                if (target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement) {
                    target.style.userSelect = "none";
                    target.style.webkitUserSelect = "none";
                }
            });
        }
        invoke() {
            const size = this.calculate_block_size();
            const layout = this.layout;
            const style = this.input_style;
            const strokeWidth = this.params.stroke_width ?? 1.5;
            const strokeOffset = strokeWidth / 2;
            const geometry = this.get_geometry();
            const notch = this.get_notch_layout(size, strokeWidth);
            const extraHeight = geometry.bottom_connector && notch.enabled ? geometry.notch.depth : 0;
            const totalWidth = size.width + strokeWidth;
            const totalHeight = size.height + extraHeight + strokeWidth;
            const svg = document.createElementNS(this.namespace, "svg");
            const path = document.createElementNS(this.namespace, "path");
            path.setAttribute("d", this.construct_block_contents(size, strokeWidth));
            path.setAttribute("fill", this.params.fill);
            path.setAttribute("stroke", this.params.stroke ?? this.calculate_stroke_color(this.params.fill));
            path.setAttribute("stroke-width", String(strokeWidth));
            path.setAttribute("stroke-linejoin", "round");
            path.setAttribute("transform", `translate(${strokeOffset} ${strokeOffset})`);
            const items = this.get_row_items();
            const contentWidth = this.measure_row(items);
            let x = layout.padding.x;
            if (layout.alignment === "center") {
                x = (size.width - contentWidth) / 2;
            }
            else if (layout.alignment === "right") {
                x = size.width - layout.padding.x - contentWidth;
            }
            const labelHeight = this.get_font_size(this.params.font) + style.padding.y * 2;
            const contentHeight = items.reduce((maximum, item) => Math.max(maximum, this.get_row_item_height(item)), labelHeight);
            const contentY = (size.height - contentHeight) / 2;
            for (let index = 0; index < items.length; index++) {
                const item = items[index];
                const itemHeight = this.get_row_item_height(item);
                const itemY = contentY + (contentHeight - itemHeight) / 2;
                if (item.kind === "label") {
                    svg.appendChild(this.create_label_element(item.segment, x + strokeOffset, itemY + strokeOffset));
                }
                else {
                    svg.appendChild(this.create_input_element(item.input, x + strokeOffset, itemY + strokeOffset, item.index));
                }
                x += this.get_row_item_width(item);
                if (index < items.length - 1)
                    x += this.get_row_gap(item, items[index + 1]);
            }
            svg.classList.add("block-vector-graphics");
            svg.dataset.blockType = String(this.type);
            svg.dataset.blockId = this.id;
            if (this.is_template) {
                svg.dataset.blockTemplate = "true";
                svg.setAttribute("aria-label", `${this.label || "Block"} toolbox template`);
            }
            else {
                delete svg.dataset.blockTemplate;
            }
            svg.setAttribute("viewBox", `0 0 ${totalWidth} ${totalHeight}`);
            svg.setAttribute("width", String(totalWidth));
            svg.setAttribute("height", String(totalHeight));
            svg.setAttribute("id", this.id);
            svg.insertBefore(path, svg.firstChild);
            this.element?.remove();
            this.element = svg;
            if (!BlockBuilderMachine.CreatedBlocks.includes(this)) {
                BlockBuilderMachine.CreatedBlocks.push(this);
            }
            this.disable_text_selection();
            this.apply_drag();
            return svg;
        }
        destroy() {
            this.clear_stack_preview();
            const children = [...this.input_connections.values()];
            this.input_connections.clear();
            for (const child of children) {
                child.parent_input = null;
                child.destroy();
            }
            const inputParent = this.parent_input;
            if (inputParent) {
                inputParent.block.input_connections.delete(inputParent.index);
                this.parent_input = null;
                inputParent.block.refresh_render_preserving_position();
            }
            this.remove_connectors();
            BlockBuilderMachine.VM.remove_block(this.id);
            BlockBuilderMachine.CreatedBlocks = BlockBuilderMachine.CreatedBlocks.filter((block) => block !== this);
            this.element?.remove();
            this.measurement_svg?.remove();
            this.measurement_svg = null;
            this.measurement_text = null;
            this.element = null;
            this.block_notches = [];
        }
        get_geometry() {
            const geometry = this.params.geometry ?? {
                radius: 5,
                notch: { width: 14, depth: 8 },
                top_notch: false,
                bottom_connector: false,
            };
            if (this.embed_radius === null)
                return geometry;
            return {
                ...geometry,
                radius: this.embed_radius,
                top_notch: false,
                bottom_connector: false,
            };
        }
        has_top_notch() {
            return this.get_geometry().top_notch && this.get_notch_layout(this.calculate_block_size(), this.params.stroke_width ?? 1.5).enabled;
        }
        has_bottom_connector() {
            return this.get_geometry().bottom_connector && this.get_notch_layout(this.calculate_block_size(), this.params.stroke_width ?? 1.5).enabled;
        }
        get_next_block() {
            return this.connections.find((connection) => connection.activated)?.connection ?? null;
        }
        get_connected_input(index) {
            return this.input_connections.get(index) ?? null;
        }
        get_parent_block() {
            return this.parent_connection;
        }
        get_stack_root() {
            let current = this;
            const visited = new Set();
            while (current.parent_connection && !visited.has(current.parent_connection)) {
                visited.add(current);
                current = current.parent_connection;
            }
            return current;
        }
        get_stack_tail() {
            let current = this;
            const visited = new Set();
            while (current.get_next_block() && !visited.has(current.get_next_block())) {
                visited.add(current);
                current = current.get_next_block();
            }
            return current;
        }
        get_stack_members() {
            const members = [];
            const visited = new Set();
            let current = this;
            while (current && !visited.has(current)) {
                visited.add(current);
                members.push(current);
                current = current.get_next_block();
            }
            return members;
        }
        collect_segment_blocks() {
            const blocks = [];
            const seen = new Set();
            const visit = (block) => {
                if (seen.has(block))
                    return;
                seen.add(block);
                blocks.push(block);
                for (const child of block.input_connections.values())
                    visit(child);
            };
            for (const block of this.get_stack_members())
                visit(block);
            return blocks;
        }
        is_reporter() {
            return this.type === CommandBlocks.REP ||
                this.type === OutputBlocks.STRING ||
                this.type === OutputBlocks.BOOL;
        }
        can_connect_to(lower_block) {
            if (this === lower_block || this.is_reporter() || lower_block.is_reporter())
                return false;
            if (!this.has_bottom_connector() || !lower_block.has_top_notch())
                return false;
            if (this.get_next_block() !== null || lower_block.parent_connection !== null)
                return false;
            if (this.get_stack_root() === lower_block.get_stack_root())
                return false;
            return true;
        }
        insert_connectors(lower_block) {
            if (!this.can_connect_to(lower_block))
                return undefined;
            const connector = {
                connection: lower_block,
                activated: true,
                compiled: false,
            };
            this.connections.push(connector);
            lower_block.parent_connection = this;
            return connector;
        }
        remove_connectors(connection) {
            const outgoing = this.connections.filter((item) => connection ? item.connection === connection : true);
            for (const item of outgoing) {
                if (item.connection.parent_connection === this) {
                    item.connection.parent_connection = null;
                }
            }
            this.connections = this.connections.filter((item) => connection ? item.connection !== connection : false);
            if (!connection || this.parent_connection === connection) {
                const parent = this.parent_connection;
                if (parent) {
                    parent.connections = parent.connections.filter((item) => item.connection !== this);
                }
                this.parent_connection = null;
            }
        }
        get_connector_world_point(side) {
            const element = this.element;
            if (!element?.isConnected)
                return null;
            const notch = this.block_notches.find((item) => item.side === side);
            if (!notch)
                return null;
            const matrix = element.getScreenCTM();
            if (!matrix)
                return null;
            const strokeOffset = (this.params.stroke_width ?? 1.5) / 2;
            const point = element.createSVGPoint();
            point.x = notch.position.x + strokeOffset;
            point.y = notch.position.y + strokeOffset;
            const screenPoint = point.matrixTransform(matrix);
            return { x: screenPoint.x, y: screenPoint.y };
        }
        get_connector_world_x() {
            return this.get_connector_world_point(this.has_top_notch() ? "top" : "bottom")?.x ?? 0;
        }
        get_body_top_world_y() {
            const element = this.element;
            if (!element)
                return 0;
            return element.getBoundingClientRect().top + (this.params.stroke_width ?? 1.5) / 2;
        }
        get_body_bottom_world_y() {
            return this.get_body_top_world_y() + this.calculate_block_size().height;
        }
        move_blocks_by(blocks, delta_x, delta_y) {
            for (const block of blocks) {
                const element = block.element;
                const parent = element?.parentElement;
                if (!element || !parent)
                    continue;
                if (getComputedStyle(parent).position === "static") {
                    parent.style.position = "relative";
                }
                const scale = BlockWorkspaceEditor.get_workspace_scale();
                const rect = element.getBoundingClientRect();
                const parentRect = parent.getBoundingClientRect();
                const left = (rect.left + delta_x - parentRect.left) / scale - parent.clientLeft + parent.scrollLeft;
                const top = (rect.top + delta_y - parentRect.top) / scale - parent.clientTop + parent.scrollTop;
                element.style.position = "absolute";
                element.style.left = `${left}px`;
                element.style.top = `${top}px`;
            }
        }
        get_workspace_root_block() {
            let current = this;
            const visited = new Set();
            while (current.parent_input && !visited.has(current.parent_input.block)) {
                visited.add(current.parent_input.block);
                current = current.parent_input.block;
            }
            return current.get_stack_root();
        }
        refresh_render_preserving_position(visited = new Set()) {
            if (visited.has(this))
                return;
            visited.add(this);
            const previous = this.element;
            if (!previous)
                return;
            const parent = previous.parentNode;
            const state = this.capture_visual_state(previous);
            const previous_height = previous.getBoundingClientRect().height;
            this.element = null;
            const updated = this.invoke();
            this.apply_visual_state(updated, state);
            if (parent) {
                parent.replaceChild(updated, previous);
            }
            else {
                previous.replaceWith(updated);
            }
            const height_delta = updated.getBoundingClientRect().height - previous_height;
            if (Math.abs(height_delta) > 0.5) {
                const downstream = this.get_stack_members().slice(1);
                if (downstream.length > 0)
                    this.move_blocks_by(downstream, 0, height_delta);
            }
            this.parent_input?.block.refresh_render_preserving_position(visited);
        }
        detach_from_input(workspace_override) {
            const parentInput = this.parent_input;
            if (!parentInput)
                return;
            this.set_embed_radius(null);
            const element = this.element;
            if (!element)
                return;
            const workspace = workspace_override
                ?? BlockWorkspaceEditor.get_workspace(element)
                ?? parentInput.block.get_workspace_root_block().element?.parentElement
                ?? null;
            if (!workspace)
                return;
            const rect = element.getBoundingClientRect();
            if (getComputedStyle(workspace).position === "static") {
                workspace.style.position = "relative";
            }
            parentInput.block.input_connections.delete(parentInput.index);
            this.parent_input = null;
            element.remove();
            workspace.appendChild(element);
            element.style.position = "absolute";
            BlockWorkspaceEditor.set_workspace_position(element, rect.left, rect.top, workspace);
            element.style.right = "auto";
            element.style.bottom = "auto";
            element.style.zIndex = "";
            element.style.display = "";
            element.style.flex = "";
            element.style.margin = "";
            element.style.cursor = "grab";
            parentInput.block.refresh_render_preserving_position();
        }
        has_input_descendant(candidate, visited = new Set()) {
            if (this === candidate)
                return true;
            if (visited.has(this))
                return false;
            visited.add(this);
            for (const child of this.input_connections.values()) {
                if (child === candidate || child.has_input_descendant(candidate, visited)) {
                    return true;
                }
            }
            return false;
        }
        find_reporter_input_drop_plan(reporter, client_x, client_y) {
            if (!reporter.is_reporter() || reporter.parent_input || !reporter.element)
                return null;
            const reporterRect = reporter.element.getBoundingClientRect();
            const reporterArea = Math.max(1, reporterRect.width * reporterRect.height);
            const candidates = [];
            const slots = Array.from(document.querySelectorAll('.block-input-slot[data-block-id][data-block-input-index]'));
            for (const slot of slots) {
                const target = BlockBuilderMachine.CreatedBlocks.find(block => block.id === slot.dataset.blockId);
                if (!target || target === reporter || target.is_template || !target.element?.isConnected)
                    continue;
                const index = Number(slot.dataset.blockInputIndex);
                if (!Number.isInteger(index) || !target.inputs[index])
                    continue;
                if (reporter.has_input_descendant(target))
                    continue;
                const rect = slot.getBoundingClientRect();
                if (rect.width <= 0 || rect.height <= 0)
                    continue;
                const expandX = Math.max(18, Math.min(48, reporterRect.width * 0.35));
                const expandY = Math.max(18, Math.min(42, reporterRect.height * 0.35));
                const dx = client_x < rect.left ? rect.left - client_x
                    : client_x > rect.right ? client_x - rect.right : 0;
                const dy = client_y < rect.top ? rect.top - client_y
                    : client_y > rect.bottom ? client_y - rect.bottom : 0;
                const pointerDistance = Math.hypot(dx, dy);
                const overlapWidth = Math.max(0, Math.min(reporterRect.right, rect.right) - Math.max(reporterRect.left, rect.left));
                const overlapHeight = Math.max(0, Math.min(reporterRect.bottom, rect.bottom) - Math.max(reporterRect.top, rect.top));
                const overlapArea = overlapWidth * overlapHeight;
                const overlapRatio = overlapArea / Math.max(1, Math.min(reporterArea, rect.width * rect.height));
                const pointerNear = dx <= expandX && dy <= expandY;
                if (!pointerNear && overlapRatio < 0.04)
                    continue;
                candidates.push({
                    kind: "reporter-input",
                    target,
                    index,
                    slot,
                    rect,
                    delta_x: rect.left - reporterRect.left,
                    delta_y: rect.top - reporterRect.top,
                    score: pointerDistance - overlapRatio * 80,
                });
            }
            candidates.sort((a, b) => a.score - b.score);
            return candidates[0] ?? null;
        }
        accepts_reporter_input_drop(reporter, client_x, client_y) {
            return this.find_reporter_input_drop_plan(reporter, client_x, client_y) !== null;
        }
        try_drop_reporter_into_input(reporter, plan) {
            if (!reporter.is_reporter() || reporter.parent_input)
                return false;
            const { target, index, slot } = plan;
            if (!target.element?.isConnected || !target.inputs[index] || !slot.isConnected)
                return false;
            const rect = slot.getBoundingClientRect();
            const previous = target.input_connections.get(index);
            if (previous) {
                const workspace = BlockWorkspaceEditor.get_workspace(target.element);
                if (!workspace)
                    return false;
                const oldElement = previous.element;
                if (oldElement) {
                    oldElement.remove();
                    workspace.appendChild(oldElement);
                    oldElement.style.position = "absolute";
                    BlockWorkspaceEditor.set_workspace_position(oldElement, rect.right + 12, rect.top, workspace);
                    oldElement.style.right = "auto";
                    oldElement.style.bottom = "auto";
                    oldElement.style.zIndex = "";
                    oldElement.style.display = "";
                    oldElement.style.flex = "";
                    oldElement.style.margin = "";
                    oldElement.style.cursor = "grab";
                }
                previous.parent_input = null;
                target.input_connections.delete(index);
            }
            target.input_connections.set(index, reporter);
            reporter.parent_input = { block: target, index };
            target.refresh_render_preserving_position();
            return true;
        }
        find_stack_drop_plan(source_root) {
            if (source_root.is_reporter() || source_root.is_template)
                return null;
            const source_blocks = source_root.get_stack_members();
            const source_set = new Set(source_blocks);
            const source_top = source_root;
            const source_bottom = source_root.get_stack_tail();
            const candidates = [...BlockBuilderMachine.CreatedBlocks].reverse();
            let best = null;
            for (const candidate of candidates) {
                if (source_set.has(candidate) || candidate.is_reporter() || candidate.is_template ||
                    !candidate.element?.isConnected)
                    continue;
                if (candidate.parent_input || candidate !== candidate.get_stack_root())
                    continue;
                const target_root = candidate;
                const target_bottom = target_root.get_stack_tail();
                if (source_bottom.can_connect_to(target_root)) {
                    const sourceAnchor = source_bottom.get_connector_world_point("bottom");
                    const targetAnchor = target_root.get_connector_world_point("top");
                    if (!sourceAnchor || !targetAnchor)
                        continue;
                    const delta_x = targetAnchor.x - sourceAnchor.x;
                    const delta_y = targetAnchor.y - sourceAnchor.y;
                    const snapRadius = Math.max(28, (source_bottom.get_geometry().notch.width + target_root.get_geometry().notch.width) * 0.75, (source_bottom.get_geometry().notch.depth + target_root.get_geometry().notch.depth) * 1.5) * BlockWorkspaceEditor.get_workspace_scale();
                    const distance = Math.hypot(delta_x, delta_y);
                    if (distance <= snapRadius) {
                        if (!best || distance < best.distance) {
                            best = { upper: source_bottom, lower: target_root, delta_x, delta_y, distance };
                        }
                    }
                }
                if (target_bottom.can_connect_to(source_top)) {
                    const targetAnchor = target_bottom.get_connector_world_point("bottom");
                    const sourceAnchor = source_top.get_connector_world_point("top");
                    if (!targetAnchor || !sourceAnchor)
                        continue;
                    const delta_x = targetAnchor.x - sourceAnchor.x;
                    const delta_y = targetAnchor.y - sourceAnchor.y;
                    const snapRadius = Math.max(28, (target_bottom.get_geometry().notch.width + source_top.get_geometry().notch.width) * 0.75, (target_bottom.get_geometry().notch.depth + source_top.get_geometry().notch.depth) * 1.5) * BlockWorkspaceEditor.get_workspace_scale();
                    const distance = Math.hypot(delta_x, delta_y);
                    if (distance <= snapRadius) {
                        if (!best || distance < best.distance) {
                            best = { upper: target_bottom, lower: source_top, delta_x, delta_y, distance };
                        }
                    }
                }
            }
            return best;
        }
        try_connect_stack(source_root) {
            const source_blocks = source_root.get_stack_members();
            const plan = source_root.find_stack_drop_plan(source_root);
            if (!plan || !plan.upper.insert_connectors(plan.lower))
                return false;
            source_root.move_blocks_by(source_blocks, plan.delta_x, plan.delta_y);
            source_root.clear_stack_preview();
            return true;
        }
        return_to_toolbox() {
            const workspace = BlockWorkspaceEditor.get_workspace(this.element);
            if (!workspace)
                return;
            const segment = this.collect_segment_blocks();
            const removed = new Set();
            for (const block of segment) {
                if (!block.is_toolbox_clone)
                    continue;
                const container = block.parent_input?.block ?? block.parent_connection;
                if (!container || removed.has(container)) {
                    removed.add(block);
                }
            }
            for (const block of segment) {
                if (removed.has(block))
                    continue;
                const container = block.parent_input?.block ?? block.parent_connection;
                if (!container || !removed.has(container))
                    continue;
                if (block.parent_input)
                    block.detach_from_input(workspace);
                if (block.parent_connection)
                    block.parent_connection.remove_connectors(block);
            }
            for (const block of [...removed].reverse()) {
                block.destroy();
            }
        }
        render_drop_preview(dragged, plan) {
            switch (plan.kind) {
                case "stack":
                case "reporter-input":
                case "toolbox-return": {
                    if (!this.drag_preview_layer) {
                        const layer = document.createElement("div");
                        layer.dataset.blockDragPreview = "true";
                        layer.setAttribute("aria-hidden", "true");
                        layer.style.position = "fixed";
                        layer.style.left = "0";
                        layer.style.top = "0";
                        layer.style.right = "0";
                        layer.style.bottom = "0";
                        layer.style.margin = "0";
                        layer.style.padding = "0";
                        layer.style.transform = "none";
                        layer.style.width = "100vw";
                        layer.style.height = "100vh";
                        layer.style.overflow = "visible";
                        layer.style.pointerEvents = "none";
                        layer.style.zIndex = "2147483000";
                        document.body.appendChild(layer);
                        this.drag_preview_layer = layer;
                    }
                    const layer = this.drag_preview_layer;
                    layer.replaceChildren();
                    const previewBlocks = plan.kind === "reporter-input" ? dragged.slice(0, 1) : dragged;
                    const preview_radius = plan.kind === "reporter-input" && plan.target
                        ? plan.target.input_style.radius + 3
                        : null;
                    for (const [index, block] of previewBlocks.entries()) {
                        const original = block.element;
                        if (!original?.isConnected)
                            continue;
                        const rect = original.getBoundingClientRect();
                        const preview = preview_radius === null
                            ? original.cloneNode(true)
                            : block.render_detached(preview_radius);
                        preview.removeAttribute("id");
                        preview.querySelectorAll("[id]").forEach((node) => node.removeAttribute("id"));
                        preview.dataset.blockDragPreviewItem = String(index);
                        preview.setAttribute("aria-hidden", "true");
                        preview.style.position = "absolute";
                        preview.style.left = `${rect.left + plan.delta_x}px`;
                        preview.style.top = `${rect.top + plan.delta_y}px`;
                        preview.style.width = `${rect.width}px`;
                        preview.style.height = `${rect.height}px`;
                        preview.style.right = "auto";
                        preview.style.bottom = "auto";
                        preview.style.margin = "0";
                        preview.style.zIndex = String(index);
                        preview.style.opacity = plan.kind === "toolbox-return" ? "0.35" : "0.62";
                        preview.style.pointerEvents = "none";
                        preview.style.cursor = "default";
                        preview.style.transform = "none";
                        layer.appendChild(preview);
                    }
                    break;
                }
                default:
                    this.clear_stack_preview();
                    break;
            }
        }
        clear_stack_preview() {
            this.drag_preview_layer?.remove();
            this.drag_preview_layer = null;
        }
        create_template_instance(event) {
            const templateElement = this.element;
            if (!templateElement)
                return;
            const workspace = BlockWorkspaceEditor.get_workspace(templateElement);
            if (!workspace)
                return;
            const templateRect = templateElement.getBoundingClientRect();
            const clone = this.clone_current_contents();
            const cloneElement = clone.element;
            if (!cloneElement) {
                clone.destroy();
                return;
            }
            if (getComputedStyle(workspace).position === "static") {
                workspace.style.position = "relative";
            }
            workspace.appendChild(cloneElement);
            cloneElement.style.position = "absolute";
            BlockWorkspaceEditor.set_workspace_position(cloneElement, templateRect.left, templateRect.top, workspace);
            cloneElement.style.zIndex = "1000";
            cloneElement.style.cursor = "grab";
            if (this.element) {
                const vb = BlockBuilderMachine.VM.get_block(this.id);
                if (vb)
                    VirtualMachineParser.parse_generation(vb);
            }
            if (!clone.begin_drag(event)) {
                clone.destroy();
            }
        }
        begin_drag(event) {
            const element = this.element;
            if (!element || event.button !== 0)
                return false;
            if (event.target instanceof Element) {
                const editable = event.target.closest("input, textarea, select, button");
                if (editable)
                    return false;
            }
            event.stopPropagation();
            this.detach_from_input();
            const previous_upper = this.parent_connection;
            if (previous_upper) {
                this.remove_connectors(previous_upper);
            }
            const root = this;
            const stack = this.get_stack_members();
            const startX = event.clientX;
            const startY = event.clientY;
            const snapshots = stack.flatMap((block, index) => {
                const blockElement = block.element;
                const parent = blockElement?.parentElement;
                if (!blockElement || !parent)
                    return [];
                if (getComputedStyle(parent).position === "static") {
                    parent.style.position = "relative";
                }
                const rect = blockElement.getBoundingClientRect();
                const parentRect = parent.getBoundingClientRect();
                const scale = BlockWorkspaceEditor.get_workspace_scale();
                const left = (rect.left - parentRect.left) / scale - parent.clientLeft + parent.scrollLeft;
                const top = (rect.top - parentRect.top) / scale - parent.clientTop + parent.scrollTop;
                const originalZIndex = blockElement.style.zIndex;
                blockElement.style.position = "absolute";
                blockElement.style.left = `${left}px`;
                blockElement.style.top = `${top}px`;
                blockElement.style.zIndex = String(1000 + index);
                blockElement.style.cursor = "grabbing";
                return [{ block, element: blockElement, left, top, originalZIndex }];
            });
            if (snapshots.length === 0)
                return false;
            let dragging = true;
            let moved = false;
            event.preventDefault();
            const move = (moveEvent) => {
                if (!dragging)
                    return;
                const deltaX = moveEvent.clientX - startX;
                const deltaY = moveEvent.clientY - startY;
                if (Math.abs(deltaX) > 2 || Math.abs(deltaY) > 2)
                    moved = true;
                const scale = BlockWorkspaceEditor.get_workspace_scale();
                for (const snapshot of snapshots) {
                    snapshot.element.style.left = `${snapshot.left + deltaX / scale}px`;
                    snapshot.element.style.top = `${snapshot.top + deltaY / scale}px`;
                }
                if (moved) {
                    if (BlockWorkspaceEditor.is_point_in_toolbox(moveEvent.clientX, moveEvent.clientY) && root.is_toolbox_clone) {
                        root.render_drop_preview(root.is_reporter() ? [root] : stack, { kind: "toolbox-return", delta_x: 0, delta_y: 0 });
                    }
                    else if (root.is_reporter()) {
                        const plan = root.find_reporter_input_drop_plan(root, moveEvent.clientX, moveEvent.clientY);
                        if (plan)
                            root.render_drop_preview([root], plan);
                        else
                            root.clear_stack_preview();
                    }
                    else {
                        const plan = root.find_stack_drop_plan(root);
                        if (plan)
                            root.render_drop_preview(stack, { kind: "stack", ...plan });
                        else
                            root.clear_stack_preview();
                    }
                }
                moveEvent.preventDefault();
            };
            const finish = (finishEvent, shouldConnect) => {
                if (!dragging)
                    return;
                dragging = false;
                window.removeEventListener("pointermove", move, true);
                window.removeEventListener("pointerup", onPointerUp, true);
                window.removeEventListener("pointercancel", onPointerCancel, true);
                const over_toolbox = root.is_toolbox_clone &&
                    BlockWorkspaceEditor.is_point_in_toolbox(finishEvent.clientX, finishEvent.clientY);
                let stack_connected = false;
                if (shouldConnect && moved && over_toolbox) {
                    root.return_to_toolbox();
                }
                else if (shouldConnect && moved) {
                    if (root.is_reporter()) {
                        const plan = root.find_reporter_input_drop_plan(root, finishEvent.clientX, finishEvent.clientY);
                        if (plan) {
                            root.try_drop_reporter_into_input(root, plan);
                        }
                    }
                    else {
                        const plan = root.find_stack_drop_plan(root);
                        if (plan && plan.upper.insert_connectors(plan.lower)) {
                            root.move_blocks_by(stack, plan.delta_x, plan.delta_y);
                            stack_connected = true;
                        }
                    }
                }
                root.clear_stack_preview();
                BlockWorkspaceEditor.refresh_content_bounds();
                for (const snapshot of snapshots) {
                    if (!snapshot.element.isConnected)
                        continue;
                    snapshot.element.style.zIndex = snapshot.originalZIndex;
                    snapshot.element.style.cursor = "grab";
                }
                if (stack_connected) {
                    const generated = VirtualMachineParser.compile_stack(root);
                    console.debug("RECOMPILED CONNECTED STACK:", generated);
                }
            };
            const onPointerUp = (upEvent) => finish(upEvent, true);
            const onPointerCancel = (cancelEvent) => finish(cancelEvent, false);
            window.addEventListener("pointermove", move, true);
            window.addEventListener("pointerup", onPointerUp, true);
            window.addEventListener("pointercancel", onPointerCancel, true);
            return true;
        }
        apply_drag() {
            const element = this.element;
            if (!element)
                return;
            element.style.touchAction = "none";
            if (this.is_template) {
                element.style.cursor = "copy";
                element.addEventListener("pointerdown", (event) => {
                    if (event.button !== 0)
                        return;
                    if (event.target instanceof Element && event.target.closest("input, textarea, select, button"))
                        return;
                    event.preventDefault();
                    event.stopPropagation();
                    this.create_template_instance(event);
                });
                return;
            }
            element.style.cursor = "grab";
            element.addEventListener("pointerdown", (event) => {
                const templateOwner = element.parentElement?.closest("[data-block-template='true']");
                if (templateOwner)
                    return;
                this.begin_drag(event);
            });
        }
    }
    BlockBuilderMachine.Block = Block;
})(BlockBuilderMachine || (BlockBuilderMachine = {}));
export var BlockWorkspaceEditor;
(function (BlockWorkspaceEditor) {
    let workspace_root = null;
    let toolbox_root = null;
    const MIN_ZOOM = 0.25;
    const MAX_ZOOM = 2.5;
    const ZOOM_STEP = 1.2;
    let viewport = { scale: 1, x: 0, y: 0 };
    const depth_installed = new WeakSet();
    let frame_root = null;
    let canvas_root = null;
    const grid_style = {
        size: 24,
        minor: "rgba(32, 41, 79, 0.10)",
        major_every: 4,
        major: "rgba(32, 41, 79, 0.22)",
        dot: "rgba(32, 41, 79, 0.30)",
    };
    let debug_enabled = true;
    let grid_enabled = true;
    let bounds_enabled = true;
    function grid_visible() {
        return grid_enabled;
    }
    function bounds_visible() {
        return debug_enabled && bounds_enabled;
    }
    let fill_height_enabled = true;
    let blur_installed = false;
    function set_workspace(target) {
        workspace_root = target;
        if (target) {
            target.dataset.blockWorkspace = "true";
            if (getComputedStyle(target).position === "static") {
                target.style.position = "relative";
            }
        }
    }
    BlockWorkspaceEditor.set_workspace = set_workspace;
    function set_toolbox(target) {
        toolbox_root = target;
        if (target)
            target.dataset.blockToolbox = "true";
    }
    BlockWorkspaceEditor.set_toolbox = set_toolbox;
    function is_point_in_toolbox(client_x, client_y) {
        const template_hosts = BlockBuilderMachine.CreatedBlocks
            .filter(block => block.is_template)
            .map(block => block.element?.parentElement ?? null);
        const candidates = [
            toolbox_root,
            ...template_hosts,
            ...Array.from(document.querySelectorAll('[data-block-toolbox], .block-toolbox, #block-panel, #block-toolbox, .block-palette-toolbox')),
        ].filter((node) => !!node?.isConnected);
        return candidates.some(node => {
            const rect = node.getBoundingClientRect();
            return client_x >= rect.left && client_x <= rect.right &&
                client_y >= rect.top && client_y <= rect.bottom;
        });
    }
    BlockWorkspaceEditor.is_point_in_toolbox = is_point_in_toolbox;
    function get_workspace(source) {
        if (canvas_root?.isConnected)
            return canvas_root;
        if (workspace_root?.isConnected)
            return workspace_root;
        const local = source?.closest("[data-block-workspace], .block-workspace, [data-editor-workspace]") ?? null;
        const marked = document.querySelector("[data-block-workspace], .block-workspace, [data-editor-workspace]");
        return local ?? marked ?? source?.parentElement ?? null;
    }
    BlockWorkspaceEditor.get_workspace = get_workspace;
    function clamp_zoom(scale) {
        return Math.min(MAX_ZOOM, Math.max(MIN_ZOOM, scale));
    }
    function get_frame() {
        return frame_root?.isConnected ? frame_root : workspace_root;
    }
    function resolve_frame(node) {
        const frame = get_frame();
        if (!node)
            return frame;
        if (node.dataset.blockCanvas === "true")
            return node.parentElement ?? frame;
        if (node.dataset.blockViewport === "true")
            return node;
        return frame ?? node;
    }
    function get_frame_rect(node) {
        const frame = resolve_frame(node);
        if (!frame)
            return new DOMRect(0, 0, 0, 0);
        const transform = frame.style.transform;
        frame.style.transform = "none";
        const rect = frame.getBoundingClientRect();
        frame.style.transform = transform;
        return rect;
    }
    function ensure_canvas(frame) {
        let canvas = frame.querySelector("[data-block-canvas]");
        if (!canvas) {
            canvas = document.createElement("div");
            canvas.dataset.blockCanvas = "true";
            Object.assign(canvas.style, {
                position: "absolute",
                left: "0",
                top: "0",
                width: "0",
                height: "0",
                overflow: "visible",
                transformOrigin: "0 0",
            });
            const existing = Array.from(frame.children);
            frame.appendChild(canvas);
            for (const child of existing) {
                if (child === canvas)
                    continue;
                if (child instanceof HTMLElement &&
                    (child.dataset.blockViewportControls || child.dataset.blockContentBoundsLabel)) {
                    continue;
                }
                canvas.appendChild(child);
            }
        }
        return canvas;
    }
    function get_canvas() {
        return canvas_root?.isConnected ? canvas_root : null;
    }
    BlockWorkspaceEditor.get_canvas = get_canvas;
    function fit_frame_to_viewport() {
        const frame = get_frame();
        if (!frame)
            return;
        const rect = frame.getBoundingClientRect();
        const available = Math.round(window.innerHeight - rect.top);
        if (available <= 0)
            return;
        const filled = frame.dataset.blockFilled === "true";
        if (!filled && rect.height >= available - 1)
            return;
        frame.dataset.blockFilled = "true";
        frame.style.height = `${available}px`;
        sync_bounds_label();
    }
    BlockWorkspaceEditor.fit_frame_to_viewport = fit_frame_to_viewport;
    function install_input_blur() {
        if (blur_installed)
            return;
        blur_installed = true;
        document.addEventListener("pointerdown", (event) => {
            const active = document.activeElement;
            if (!(active instanceof HTMLInputElement) && !(active instanceof HTMLTextAreaElement))
                return;
            if (!active.classList.contains("block-input"))
                return;
            const target = event.target;
            if (target instanceof Node && (target === active || active.contains(target)))
                return;
            if (target instanceof Element && target.closest("input, textarea, select"))
                return;
            active.blur();
        }, true);
    }
    function ensure_viewport_frame(workspace) {
        const parent = workspace.parentElement;
        if (parent && workspace.clientWidth === 0) {
            if (getComputedStyle(parent).display.includes("flex")) {
                workspace.style.flex = "1 1 auto";
                workspace.style.minWidth = "0";
            }
            else {
                workspace.style.width = "100%";
            }
        }
        if (parent && workspace.clientHeight === 0) {
            workspace.style.height = "100%";
        }
        return workspace.clientWidth > 0 && workspace.clientHeight > 0;
    }
    function apply_viewport_transform() {
        const canvas = canvas_root?.isConnected ? canvas_root : null;
        const frame = get_frame();
        if (canvas) {
            canvas.style.transformOrigin = "0 0";
            canvas.style.transform =
                `translate(${viewport.x}px, ${viewport.y}px) scale(${viewport.scale})`;
        }
        else if (frame) {
            frame.style.transformOrigin = "0 0";
            frame.style.transform =
                `translate(${viewport.x}px, ${viewport.y}px) scale(${viewport.scale})`;
        }
        sync_viewport_grid();
        sync_bounds_label();
        sync_viewport_readout();
    }
    function sync_viewport_grid() {
        const workspace = get_frame();
        if (!workspace)
            return;
        workspace.style.outline = debug_enabled ? "1px dashed rgba(32, 41, 79, 0.35)" : "";
        workspace.style.outlineOffset = debug_enabled ? "-1px" : "";
        if (!grid_visible()) {
            workspace.style.backgroundImage = "";
            workspace.style.backgroundSize = "";
            workspace.style.backgroundPosition = "";
            return;
        }
        const scale = viewport.scale || 1;
        const minor = grid_style.size * scale;
        const major = minor * grid_style.major_every;
        if (!debug_enabled) {
            workspace.style.backgroundImage =
                `radial-gradient(circle, ${grid_style.dot} 1px, transparent 1px)`;
            workspace.style.backgroundSize = `${minor}px ${minor}px`;
            workspace.style.backgroundPosition = `${viewport.x}px ${viewport.y}px`;
            return;
        }
        workspace.style.backgroundImage = [
            `linear-gradient(to right, ${grid_style.major} 1px, transparent 1px)`,
            `linear-gradient(to bottom, ${grid_style.major} 1px, transparent 1px)`,
            `linear-gradient(to right, ${grid_style.minor} 1px, transparent 1px)`,
            `linear-gradient(to bottom, ${grid_style.minor} 1px, transparent 1px)`,
        ].join(", ");
        workspace.style.backgroundSize = [
            `${major}px ${major}px`,
            `${major}px ${major}px`,
            `${minor}px ${minor}px`,
            `${minor}px ${minor}px`,
        ].join(", ");
        workspace.style.backgroundPosition = `${viewport.x}px ${viewport.y}px`;
    }
    function set_grid(visible, options = {}) {
        grid_enabled = visible;
        Object.assign(grid_style, options);
        sync_viewport_grid();
    }
    BlockWorkspaceEditor.set_grid = set_grid;
    function set_debug(visible) {
        debug_enabled = visible;
        sync_viewport_grid();
        refresh_content_bounds();
    }
    BlockWorkspaceEditor.set_debug = set_debug;
    function set_content_bounds(visible) {
        bounds_enabled = visible;
        refresh_content_bounds();
    }
    BlockWorkspaceEditor.set_content_bounds = set_content_bounds;
    function get_debug() {
        return debug_enabled;
    }
    BlockWorkspaceEditor.get_debug = get_debug;
    let bounds_world = null;
    function ensure_bounds_label(frame) {
        let label = frame.querySelector("[data-block-content-bounds-label]");
        if (!label) {
            label = document.createElement("div");
            label.dataset.blockContentBoundsLabel = "true";
            Object.assign(label.style, {
                position: "absolute",
                left: "0",
                top: "0",
                zIndex: "2147482000",
                pointerEvents: "none",
                padding: "2px 6px",
                border: "1px solid #d14343",
                borderRadius: "4px",
                background: "#ffffff",
                color: "#d14343",
                font: "11px/1.3 Arial, sans-serif",
                whiteSpace: "nowrap",
                display: "none",
            });
            frame.appendChild(label);
        }
        return label;
    }
    function sync_bounds_label() {
        const frame = get_frame();
        if (!frame)
            return;
        const label = frame.querySelector("[data-block-content-bounds-label]");
        if (!label)
            return;
        if (!bounds_world) {
            label.style.display = "none";
            return;
        }
        const scale = viewport.scale || 1;
        const { x, y, width, height } = bounds_world;
        const left = viewport.x + x * scale;
        const top = viewport.y + y * scale;
        const bottom = viewport.y + (y + height) * scale;
        const label_width = label.offsetWidth || 84;
        const label_height = label.offsetHeight || 20;
        const above = top - label_height - 2;
        const below = bottom + 2;
        let label_top;
        if (above >= 0) {
            label_top = above;
        }
        else if (below + label_height <= frame.clientHeight) {
            label_top = below;
        }
        else {
            label_top = Math.max(0, Math.min(top + 2, frame.clientHeight - label_height));
        }
        label.style.display = "block";
        label.style.left = `${Math.max(0, Math.min(left, frame.clientWidth - label_width))}px`;
        label.style.top = `${label_top}px`;
    }
    function refresh_content_bounds() {
        const workspace = get_frame();
        const world = canvas_root?.isConnected ? canvas_root : workspace;
        if (!workspace || !world)
            return;
        if (!bounds_visible()) {
            const hidden = world.querySelector("[data-block-content-bounds]");
            if (hidden)
                hidden.style.display = "none";
            bounds_world = null;
            sync_bounds_label();
            return;
        }
        let node = world.querySelector("[data-block-content-bounds]");
        if (!node) {
            node = document.createElement("div");
            node.dataset.blockContentBounds = "true";
            Object.assign(node.style, {
                position: "absolute",
                pointerEvents: "none",
                zIndex: "2147482000",
                border: "1px dashed #d14343",
                borderRadius: "4px",
                boxSizing: "border-box",
                display: "none",
            });
            world.appendChild(node);
        }
        const blocks = BlockBuilderMachine.CreatedBlocks.filter(block => block.element?.isConnected && !block.is_template);
        if (blocks.length === 0) {
            node.style.display = "none";
            bounds_world = null;
            sync_bounds_label();
            return;
        }
        const scale = viewport.scale || 1;
        const frame = get_frame_rect(workspace);
        let min_x = Number.POSITIVE_INFINITY;
        let min_y = Number.POSITIVE_INFINITY;
        let max_x = Number.NEGATIVE_INFINITY;
        let max_y = Number.NEGATIVE_INFINITY;
        for (const block of blocks) {
            const element = block.element;
            if (!element)
                continue;
            const rect = element.getBoundingClientRect();
            const left = (rect.left - frame.left - viewport.x) / scale;
            const top = (rect.top - frame.top - viewport.y) / scale;
            min_x = Math.min(min_x, left);
            min_y = Math.min(min_y, top);
            max_x = Math.max(max_x, left + rect.width / scale);
            max_y = Math.max(max_y, top + rect.height / scale);
        }
        if (!Number.isFinite(min_x) || !Number.isFinite(min_y))
            return;
        bounds_world = {
            x: min_x,
            y: min_y,
            width: max_x - min_x,
            height: max_y - min_y,
        };
        node.style.display = "block";
        node.style.left = `${min_x}px`;
        node.style.top = `${min_y}px`;
        node.style.width = `${bounds_world.width}px`;
        node.style.height = `${bounds_world.height}px`;
        const label = ensure_bounds_label(workspace);
        label.textContent =
            `${Math.round(bounds_world.width)} \u00d7 ${Math.round(bounds_world.height)} px`;
        sync_bounds_label();
    }
    BlockWorkspaceEditor.refresh_content_bounds = refresh_content_bounds;
    function sync_viewport_readout() {
        const readout = document.querySelector("[data-block-zoom-readout]");
        if (readout)
            readout.textContent = `${Math.round(viewport.scale * 100)}%`;
    }
    function get_zoom() {
        return viewport.scale;
    }
    BlockWorkspaceEditor.get_zoom = get_zoom;
    function get_workspace_scale() {
        return viewport.scale || 1;
    }
    BlockWorkspaceEditor.get_workspace_scale = get_workspace_scale;
    function to_workspace_delta(value) {
        return value / (viewport.scale || 1);
    }
    BlockWorkspaceEditor.to_workspace_delta = to_workspace_delta;
    function set_workspace_position(element, client_left, client_top, workspace = workspace_root) {
        if (!workspace)
            return;
        if (getComputedStyle(workspace).position === "static") {
            workspace.style.position = "relative";
        }
        const frame = get_frame_rect(workspace);
        const scale = viewport.scale || 1;
        element.style.left = `${(client_left - frame.left - viewport.x) / scale - workspace.clientLeft + workspace.scrollLeft}px`;
        element.style.top = `${(client_top - frame.top - viewport.y) / scale - workspace.clientTop + workspace.scrollTop}px`;
    }
    BlockWorkspaceEditor.set_workspace_position = set_workspace_position;
    function set_zoom(scale, client_x, client_y) {
        const workspace = workspace_root;
        if (!workspace)
            return;
        const next = clamp_zoom(scale);
        if (Math.abs(next - viewport.scale) < 0.001)
            return;
        const frame = get_frame_rect(workspace);
        const framed = frame.width > 0 && frame.height > 0;
        const anchor_x = client_x ?? (framed ? frame.left + frame.width / 2 : frame.left);
        const anchor_y = client_y ?? (framed ? frame.top + frame.height / 2 : frame.top);
        const world_x = (anchor_x - frame.left - viewport.x) / viewport.scale;
        const world_y = (anchor_y - frame.top - viewport.y) / viewport.scale;
        viewport.scale = next;
        viewport.x = anchor_x - frame.left - next * world_x;
        viewport.y = anchor_y - frame.top - next * world_y;
        apply_viewport_transform();
    }
    BlockWorkspaceEditor.set_zoom = set_zoom;
    function zoom_in(factor = ZOOM_STEP) {
        set_zoom(viewport.scale * factor);
    }
    BlockWorkspaceEditor.zoom_in = zoom_in;
    function zoom_out(factor = ZOOM_STEP) {
        set_zoom(viewport.scale / factor);
    }
    BlockWorkspaceEditor.zoom_out = zoom_out;
    function pan_by(delta_x, delta_y) {
        viewport.x += delta_x;
        viewport.y += delta_y;
        apply_viewport_transform();
    }
    BlockWorkspaceEditor.pan_by = pan_by;
    function center_viewport(padding = 32) {
        const workspace = workspace_root;
        if (!workspace)
            return;
        const frame = get_frame_rect(workspace);
        const scale = viewport.scale || 1;
        const blocks = BlockBuilderMachine.CreatedBlocks.filter(block => block.element?.isConnected && !block.is_template);
        if (blocks.length === 0 || frame.width <= 0 || frame.height <= 0) {
            viewport.x = 0;
            viewport.y = 0;
            apply_viewport_transform();
            return;
        }
        let min_x = Number.POSITIVE_INFINITY;
        let min_y = Number.POSITIVE_INFINITY;
        let max_x = Number.NEGATIVE_INFINITY;
        let max_y = Number.NEGATIVE_INFINITY;
        for (const block of blocks) {
            const element = block.element;
            if (!element)
                continue;
            const rect = element.getBoundingClientRect();
            const left = (rect.left - frame.left - viewport.x) / scale;
            const top = (rect.top - frame.top - viewport.y) / scale;
            min_x = Math.min(min_x, left);
            min_y = Math.min(min_y, top);
            max_x = Math.max(max_x, left + rect.width / scale);
            max_y = Math.max(max_y, top + rect.height / scale);
        }
        if (!Number.isFinite(min_x) || !Number.isFinite(min_y))
            return;
        const usable_width = Math.max(0, frame.width - padding * 2);
        const usable_height = Math.max(0, frame.height - padding * 2);
        viewport.x = padding + (usable_width - (max_x - min_x) * scale) / 2 - min_x * scale;
        viewport.y = padding + (usable_height - (max_y - min_y) * scale) / 2 - min_y * scale;
        apply_viewport_transform();
        refresh_content_bounds();
    }
    BlockWorkspaceEditor.center_viewport = center_viewport;
    function mount_viewport_controls(host = null) {
        const target = host ?? get_frame() ?? workspace_root?.parentElement ?? null;
        if (!target)
            return;
        if (getComputedStyle(target).position === "static") {
            target.style.position = "relative";
        }
        let bar = document.querySelector("[data-block-viewport-controls]");
        if (!bar) {
            bar = document.createElement("div");
            bar.dataset.blockViewportControls = "true";
            bar.className = "block-viewport-controls";
            Object.assign(bar.style, {
                position: "absolute",
                left: "12px",
                bottom: "12px",
                display: "flex",
                alignItems: "center",
                gap: "4px",
                zIndex: "2147482000",
                padding: "4px",
                borderRadius: "8px",
                background: "#ffffff",
                border: "1px solid #d6dbe8",
                boxShadow: "0 1px 3px rgba(16, 24, 40, 0.14)",
            });
            const make_button = (label, title, action, wide = false) => {
                const button = document.createElement("button");
                button.type = "button";
                button.textContent = label;
                button.title = title;
                button.setAttribute("aria-label", title);
                Object.assign(button.style, {
                    width: wide ? "auto" : "30px",
                    minWidth: "30px",
                    height: "30px",
                    padding: wide ? "0 10px" : "0",
                    cursor: "pointer",
                    border: "1px solid #d6dbe8",
                    borderRadius: "6px",
                    background: "#f7f8fc",
                    color: "#20294f",
                    font: wide ? "11px/1 Arial, sans-serif" : "15px/1 Arial, sans-serif",
                    letterSpacing: wide ? "0.04em" : "",
                    whiteSpace: "nowrap",
                });
                button.addEventListener("click", (event) => {
                    event.stopPropagation();
                    action();
                });
                bar.appendChild(button);
            };
            make_button("+", "Zoom in", () => zoom_in());
            make_button("\u2212", "Zoom out", () => zoom_out());
            const readout = document.createElement("span");
            readout.dataset.blockZoomReadout = "true";
            Object.assign(readout.style, {
                minWidth: "44px",
                textAlign: "center",
                font: "12px Arial, sans-serif",
                color: "#20294f",
            });
            bar.appendChild(readout);
            make_button("CENTER", "Center viewport", () => center_viewport(), true);
        }
        if (bar.parentElement !== target) {
            target.appendChild(bar);
        }
        sync_viewport_readout();
    }
    BlockWorkspaceEditor.mount_viewport_controls = mount_viewport_controls;
    function apply_editor_depth(target = workspace_root, options = {}) {
        const workspace = target ?? workspace_root;
        if (!workspace)
            return;
        frame_root = workspace;
        if (getComputedStyle(workspace).position === "static") {
            workspace.style.position = "relative";
        }
        workspace.dataset.blockViewport = "true";
        workspace.style.touchAction = "none";
        canvas_root = ensure_canvas(workspace);
        const framed = ensure_viewport_frame(workspace);
        const clip = options.clip ?? true;
        debug_enabled = options.debug ?? true;
        grid_enabled = options.grid ?? true;
        bounds_enabled = options.content_bounds ?? options.box_size_outline ?? true;
        fill_height_enabled = options.fill_height ?? true;
        if (fill_height_enabled)
            fit_frame_to_viewport();
        install_input_blur();
        workspace.style.overflow = clip && framed ? "hidden" : "visible";
        if (!framed) {
            console.debug("[block-editor] viewport frame has no size yet, so it is not clipped. " +
                "Give the canvas a width/height (or a sized flex parent) to clip it.");
        }
        apply_viewport_transform();
        refresh_content_bounds();
        if (!depth_installed.has(workspace)) {
            depth_installed.add(workspace);
            let panning = null;
            workspace.addEventListener("pointerdown", (event) => {
                if (event.button !== 0 && event.button !== 1)
                    return;
                if (event.target instanceof Element &&
                    event.target.closest("input, textarea, select, button, [data-block-viewport-controls]")) {
                    return;
                }
                panning = {
                    pointer_id: event.pointerId,
                    x: event.clientX,
                    y: event.clientY,
                    origin_x: viewport.x,
                    origin_y: viewport.y,
                };
                workspace.style.cursor = "grabbing";
                event.preventDefault();
            });
            window.addEventListener("pointermove", (event) => {
                if (!panning)
                    return;
                if (panning.pointer_id !== undefined && event.pointerId !== undefined &&
                    event.pointerId !== panning.pointer_id)
                    return;
                viewport.x = panning.origin_x + (event.clientX - panning.x);
                viewport.y = panning.origin_y + (event.clientY - panning.y);
                apply_viewport_transform();
            }, true);
            const end_pan = () => {
                if (!panning)
                    return;
                panning = null;
                workspace.style.cursor = "";
            };
            window.addEventListener("pointerup", end_pan, true);
            window.addEventListener("pointercancel", end_pan, true);
            window.addEventListener("resize", () => {
                if (fill_height_enabled)
                    fit_frame_to_viewport();
                apply_viewport_transform();
            });
            workspace.addEventListener("wheel", (event) => {
                event.preventDefault();
                if (event.ctrlKey || event.metaKey) {
                    set_zoom(viewport.scale * Math.pow(0.9985, event.deltaY), event.clientX, event.clientY);
                    return;
                }
                viewport.x -= event.deltaX;
                viewport.y -= event.deltaY;
                apply_viewport_transform();
            }, { passive: false });
        }
        mount_viewport_controls();
    }
    BlockWorkspaceEditor.apply_editor_depth = apply_editor_depth;
})(BlockWorkspaceEditor || (BlockWorkspaceEditor = {}));
