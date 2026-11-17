import { VirtualMachine } from './blbvm.js';
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
        connecting;
        block_notches;
        element;
        inputs;
        label;
        id;
        connections;
        parent_connection = null;
        input_connections = new Map();
        parent_input = null;
        is_toolbox_clone = false;
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
            this.element = null;
            this.connecting = false;
            this.label = text.trim();
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
        refresh_input_layout(input) {
            const previous = this.element;
            if (!previous)
                return;
            const parent = previous.parentNode;
            if (!parent)
                return;
            const index = input.dataset.inputIndex;
            const selectionStart = input.selectionStart;
            const selectionEnd = input.selectionEnd;
            const selectionDirection = input.selectionDirection;
            const position = {
                position: previous.style.position,
                left: previous.style.left,
                top: previous.style.top,
            };
            this.element = null;
            const updated = this.invoke();
            updated.style.position = position.position;
            updated.style.left = position.left;
            updated.style.top = position.top;
            parent.replaceChild(updated, previous);
            const replacement = updated.querySelector(`input[data-input-index="${index}"]`);
            if (replacement) {
                replacement.focus();
                if (selectionStart !== null && selectionEnd !== null && replacement.type !== "checkbox") {
                    replacement.setSelectionRange(selectionStart, selectionEnd, selectionDirection ?? undefined);
                }
            }
            this.parent_input?.block.refresh_render_preserving_position();
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
        add_param(input) {
            this.validate_input(input);
            this.inputs.push({ ...input });
            if (this.element) {
                const previous = this.element;
                const parent = previous.parentNode;
                this.element = null;
                const updated = this.invoke();
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
            if (input.type === InputTypes.BOOL) {
                return Math.max(input.width ?? 24, 24);
            }
            if (input.type === InputTypes.BLOCK) {
                const connected = this.input_connections.get(index);
                if (connected) {
                    return Math.max(input.width ?? 0, connected.get_render_size().width);
                }
                return Math.max(input.width ?? 0, 44);
            }
            const style = this.input_style;
            const contentWidth = this.measure_text(this.get_input_display_value(input), style.font);
            const automaticWidth = contentWidth + style.padding.x * 2 + 4;
            return Math.max(input.width ?? 0, automaticWidth, 40);
        }
        get_input_height(input, index) {
            if (input.type === InputTypes.BLOCK) {
                const connected = this.input_connections.get(index);
                if (connected) {
                    return connected.get_render_size().height;
                }
            }
            const style = this.input_style;
            return this.get_font_size(style.font) + style.padding.y * 2;
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
        calculate_block_size() {
            const layout = this.layout;
            const style = this.input_style;
            const segments = this.get_text_segments(this.label);
            const segmentWidths = segments.map(segment => this.get_segment_width(segment));
            const inputWidths = this.inputs.map((input, index) => this.get_input_width(input, index));
            const inputHeights = this.inputs.map((input, index) => this.get_input_height(input, index));
            const labelSpacing = Math.max(0, segments.length - 1) * layout.spacing;
            const inputSpacing = Math.max(0, inputWidths.length - 1) * layout.input_spacing;
            const labelInputSpacing = segments.length > 0 && inputWidths.length > 0 ? layout.input_spacing : 0;
            const contentWidth = segmentWidths.reduce((total, width) => total + width, 0)
                + inputWidths.reduce((total, width) => total + width, 0)
                + labelSpacing + inputSpacing + labelInputSpacing;
            const segmentHeight = this.get_font_size(this.params.font) + style.padding.y * 2;
            const inputHeight = inputHeights.reduce((maximum, height) => Math.max(maximum, height), 0);
            return {
                width: Math.max(48, contentWidth + layout.padding.x * 2),
                height: Math.max(48, segmentHeight + layout.padding.y * 2, inputHeight + layout.padding.y * 2),
            };
        }
        get_notch_layout(size, strokeWidth) {
            const geometry = this.params.geometry ?? {
                radius: 5,
                notch: { width: 14, depth: 8 },
                top_notch: false,
                bottom_connector: false,
            };
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
            const geometry = this.params.geometry ?? {
                radius: 5,
                notch: { width: 14, depth: 8 },
                top_notch: false,
                bottom_connector: false,
            };
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
            if (input.type === InputTypes.BLOCK) {
                const connected = this.input_connections.get(index);
                container.className = "block-input-slot";
                container.dataset.blockId = this.id;
                container.dataset.blockInputIndex = String(index);
                container.dataset.inputType = "block";
                container.style.border = connected ? "none" : `${style.stroke_width}px dashed ${style.stroke}`;
                container.style.borderRadius = `${style.radius + 3}px`;
                container.style.background = connected ? "transparent" : style.fill;
                container.style.color = style.text_fill;
                container.style.font = style.font;
                container.style.padding = connected ? "0" : `${style.padding.y}px ${style.padding.x}px`;
                container.style.overflow = "visible";
                container.style.whiteSpace = "nowrap";
                if (connected?.element) {
                    connected.element.style.position = "relative";
                    connected.element.style.left = "";
                    connected.element.style.top = "";
                    connected.element.style.zIndex = "";
                    connected.element.style.display = "block";
                    connected.element.style.flex = "none";
                    connected.element.style.margin = "0";
                    connected.element.style.cursor = "grab";
                    container.appendChild(connected.element);
                }
                else {
                    container.textContent = String(input.content || "...");
                }
                foreignObject.setAttribute("x", String(x));
                foreignObject.setAttribute("y", String(y));
                foreignObject.setAttribute("width", String(width));
                foreignObject.setAttribute("height", String(height));
                foreignObject.appendChild(container);
                group.appendChild(foreignObject);
                group.dataset.inputType = "block";
                return group;
            }
            const element = document.createElement("input");
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
            element.dataset.inputType = InputTypes[input.type].toLowerCase();
            element.dataset.inputIndex = String(index);
            element.style.boxSizing = "border-box";
            element.style.border = `${style.stroke_width}px solid ${style.stroke}`;
            element.style.borderRadius = `${style.radius}px`;
            element.style.background = style.fill;
            element.style.color = style.text_fill;
            element.style.font = style.font;
            element.style.outline = "none";
            element.style.userSelect = "text";
            element.style.webkitUserSelect = "text";
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
                this.refresh_input_layout(element);
            });
            element.addEventListener("change", () => {
                if (input.type === InputTypes.BOOL) {
                    input.content = element.checked;
                }
            });
            container.appendChild(element);
            foreignObject.setAttribute("x", String(x));
            foreignObject.setAttribute("y", String(y));
            foreignObject.setAttribute("width", String(width));
            foreignObject.setAttribute("height", String(height));
            foreignObject.appendChild(container);
            group.appendChild(foreignObject);
            group.dataset.inputType = InputTypes[input.type].toLowerCase();
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
            const geometry = this.params.geometry ?? {
                radius: 5,
                notch: { width: 14, depth: 8 },
                top_notch: false,
                bottom_connector: false,
            };
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
            const segments = this.get_text_segments(this.label);
            const segmentWidths = segments.map(segment => this.get_segment_width(segment));
            const inputWidths = this.inputs.map((input, index) => this.get_input_width(input, index));
            const labelSpacing = Math.max(0, segments.length - 1) * layout.spacing;
            const inputSpacing = Math.max(0, inputWidths.length - 1) * layout.input_spacing;
            const labelInputSpacing = segments.length > 0 && inputWidths.length > 0 ? layout.input_spacing : 0;
            const contentWidth = segmentWidths.reduce((total, width) => total + width, 0)
                + inputWidths.reduce((total, width) => total + width, 0)
                + labelSpacing + inputSpacing + labelInputSpacing;
            let x = layout.padding.x;
            if (layout.alignment === "center") {
                x = (size.width - contentWidth) / 2;
            }
            else if (layout.alignment === "right") {
                x = size.width - layout.padding.x - contentWidth;
            }
            const labelHeight = this.get_font_size(this.params.font) + style.padding.y * 2;
            const inputHeights = this.inputs.map((input, index) => this.get_input_height(input, index));
            const contentHeight = Math.max(labelHeight, ...inputHeights, 0);
            const contentY = (size.height - contentHeight) / 2;
            for (let index = 0; index < segments.length; index++) {
                const segment = segments[index];
                svg.appendChild(this.create_label_element(segment, x + strokeOffset, contentY + strokeOffset));
                x += segmentWidths[index];
                if (index < segments.length - 1) {
                    x += layout.spacing;
                }
                else if (this.inputs.length > 0) {
                    x += layout.input_spacing;
                }
            }
            for (let index = 0; index < this.inputs.length; index++) {
                const input = this.inputs[index];
                svg.appendChild(this.create_input_element(input, x + strokeOffset, contentY + strokeOffset, index));
                x += inputWidths[index];
                if (index < this.inputs.length - 1) {
                    x += layout.input_spacing;
                }
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
            for (const child of [...this.input_connections.values()]) {
                child.destroy();
            }
            this.input_connections.clear();
            const inputParent = this.parent_input;
            if (inputParent) {
                inputParent.block.input_connections.delete(inputParent.index);
                this.parent_input = null;
                inputParent.block.refresh_render_preserving_position();
            }
            this.remove_connectors();
            BlockBuilderMachine.CreatedBlocks = BlockBuilderMachine.CreatedBlocks.filter((block) => block !== this);
            this.element?.remove();
            this.measurement_svg?.remove();
            this.measurement_svg = null;
            this.measurement_text = null;
            this.element = null;
        }
        get_geometry() {
            return this.params.geometry ?? {
                radius: 5,
                notch: { width: 14, depth: 8 },
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
                const rect = element.getBoundingClientRect();
                const parentRect = parent.getBoundingClientRect();
                const left = rect.left + delta_x - parentRect.left - parent.clientLeft + parent.scrollLeft;
                const top = rect.top + delta_y - parentRect.top - parent.clientTop + parent.scrollTop;
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
            const position = {
                position: previous.style.position,
                left: previous.style.left,
                top: previous.style.top,
                right: previous.style.right,
                bottom: previous.style.bottom,
                zIndex: previous.style.zIndex,
                transform: previous.style.transform,
            };
            this.element = null;
            const updated = this.invoke();
            updated.style.position = position.position;
            updated.style.left = position.left;
            updated.style.top = position.top;
            updated.style.right = position.right;
            updated.style.bottom = position.bottom;
            updated.style.zIndex = position.zIndex;
            updated.style.transform = position.transform;
            if (parent) {
                parent.replaceChild(updated, previous);
            }
            else {
                previous.replaceWith(updated);
            }
            this.parent_input?.block.refresh_render_preserving_position(visited);
        }
        detach_from_input() {
            const parentInput = this.parent_input;
            const element = this.element;
            if (!parentInput || !element)
                return;
            const rect = element.getBoundingClientRect();
            const workspaceRoot = parentInput.block.get_workspace_root_block();
            const workspace = workspaceRoot.element?.parentElement;
            if (!workspace)
                return;
            if (getComputedStyle(workspace).position === "static") {
                workspace.style.position = "relative";
            }
            parentInput.block.input_connections.delete(parentInput.index);
            this.parent_input = null;
            element.remove();
            workspace.appendChild(element);
            const workspaceRect = workspace.getBoundingClientRect();
            element.style.position = "absolute";
            element.style.left = `${rect.left - workspaceRect.left - workspace.clientLeft + workspace.scrollLeft}px`;
            element.style.top = `${rect.top - workspaceRect.top - workspace.clientTop + workspace.scrollTop}px`;
            element.style.zIndex = "";
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
            const slots = Array.from(document.querySelectorAll('.block-input-slot[data-input-type="block"][data-block-id][data-block-input-index]'));
            for (const slot of slots) {
                const target = BlockBuilderMachine.CreatedBlocks.find(block => block.id === slot.dataset.blockId);
                if (!target || target === reporter || target.is_template || !target.element?.isConnected)
                    continue;
                const index = Number(slot.dataset.blockInputIndex);
                if (!Number.isInteger(index) || target.inputs[index]?.type !== InputTypes.BLOCK)
                    continue;
                if (!target.is_reporter() && target !== target.get_stack_root())
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
            if (!target.element?.isConnected || target.inputs[index]?.type !== InputTypes.BLOCK || !slot.isConnected)
                return false;
            const rect = slot.getBoundingClientRect();
            const previous = target.input_connections.get(index);
            if (previous) {
                const workspace = BlockWorkspaceEditor.get_workspace(target.element);
                if (!workspace)
                    return false;
                const oldElement = previous.element;
                if (oldElement) {
                    const workspaceRect = workspace.getBoundingClientRect();
                    oldElement.remove();
                    workspace.appendChild(oldElement);
                    oldElement.style.position = "absolute";
                    oldElement.style.left = `${rect.left - workspaceRect.left - workspace.clientLeft + workspace.scrollLeft}px`;
                    oldElement.style.top = `${rect.top - workspaceRect.top - workspace.clientTop + workspace.scrollTop}px`;
                    oldElement.style.zIndex = "";
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
                    const snapRadius = Math.max(28, (source_bottom.get_geometry().notch.width + target_root.get_geometry().notch.width) * 0.75, (source_bottom.get_geometry().notch.depth + target_root.get_geometry().notch.depth) * 1.5);
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
                    const snapRadius = Math.max(28, (target_bottom.get_geometry().notch.width + source_top.get_geometry().notch.width) * 0.75, (target_bottom.get_geometry().notch.depth + source_top.get_geometry().notch.depth) * 1.5);
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
                    for (const [index, block] of previewBlocks.entries()) {
                        const original = block.element;
                        if (!original?.isConnected)
                            continue;
                        const rect = original.getBoundingClientRect();
                        const preview = original.cloneNode(true);
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
            const workspaceRect = workspace.getBoundingClientRect();
            cloneElement.style.position = "absolute";
            cloneElement.style.left = `${templateRect.left - workspaceRect.left - workspace.clientLeft + workspace.scrollLeft}px`;
            cloneElement.style.top = `${templateRect.top - workspaceRect.top - workspace.clientTop + workspace.scrollTop}px`;
            cloneElement.style.zIndex = "1000";
            cloneElement.style.cursor = "grab";
            clone.begin_drag(event);
        }
        begin_drag(event) {
            const element = this.element;
            if (!element || event.button !== 0)
                return;
            if (event.target instanceof Element) {
                const editable = event.target.closest("input, textarea, select, button");
                if (editable)
                    return;
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
                const left = rect.left - parentRect.left - parent.clientLeft + parent.scrollLeft;
                const top = rect.top - parentRect.top - parent.clientTop + parent.scrollTop;
                const originalZIndex = blockElement.style.zIndex;
                blockElement.style.position = "absolute";
                blockElement.style.left = `${left}px`;
                blockElement.style.top = `${top}px`;
                blockElement.style.zIndex = String(1000 + index);
                blockElement.style.cursor = "grabbing";
                return [{ block, element: blockElement, left, top, originalZIndex }];
            });
            if (snapshots.length === 0)
                return;
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
                for (const snapshot of snapshots) {
                    snapshot.element.style.left = `${snapshot.left + deltaX}px`;
                    snapshot.element.style.top = `${snapshot.top + deltaY}px`;
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
                if (shouldConnect && moved) {
                    if (root.is_toolbox_clone && BlockWorkspaceEditor.is_point_in_toolbox(finishEvent.clientX, finishEvent.clientY)) {
                        const returned = root.is_reporter() ? [root] : root.get_stack_members();
                        for (const block of returned)
                            block.destroy();
                        root.clear_stack_preview();
                        return;
                    }
                    if (root.is_reporter()) {
                        const plan = root.find_reporter_input_drop_plan(root, finishEvent.clientX, finishEvent.clientY);
                        if (plan)
                            root.try_drop_reporter_into_input(root, plan);
                    }
                    else {
                        const plan = root.find_stack_drop_plan(root);
                        if (plan && plan.upper.insert_connectors(plan.lower)) {
                            root.move_blocks_by(stack, plan.delta_x, plan.delta_y);
                        }
                    }
                }
                root.clear_stack_preview();
                for (const snapshot of snapshots) {
                    snapshot.element.style.zIndex = snapshot.originalZIndex;
                    snapshot.element.style.cursor = "grab";
                }
            };
            const onPointerUp = (upEvent) => finish(upEvent, true);
            const onPointerCancel = (cancelEvent) => finish(cancelEvent, false);
            window.addEventListener("pointermove", move, true);
            window.addEventListener("pointerup", onPointerUp, true);
            window.addEventListener("pointercancel", onPointerCancel, true);
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
        const candidates = [
            toolbox_root,
            ...Array.from(document.querySelectorAll('[data-block-toolbox], .block-toolbox, #block-panel')),
        ].filter((node) => !!node?.isConnected);
        return candidates.some(node => {
            const rect = node.getBoundingClientRect();
            return client_x >= rect.left && client_x <= rect.right &&
                client_y >= rect.top && client_y <= rect.bottom;
        });
    }
    BlockWorkspaceEditor.is_point_in_toolbox = is_point_in_toolbox;
    function get_workspace(source) {
        if (workspace_root?.isConnected)
            return workspace_root;
        const local = source?.closest("[data-block-workspace], .block-workspace, [data-editor-workspace]") ?? null;
        const marked = document.querySelector("[data-block-workspace], .block-workspace, [data-editor-workspace]");
        return local ?? marked ?? source?.parentElement ?? null;
    }
    BlockWorkspaceEditor.get_workspace = get_workspace;
    function apply_editor_depth(_target) {
    }
    BlockWorkspaceEditor.apply_editor_depth = apply_editor_depth;
})(BlockWorkspaceEditor || (BlockWorkspaceEditor = {}));
