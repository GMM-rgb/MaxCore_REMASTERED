export namespace BlockBuilderMachine {
    export type OutputBlockTypes = CommandBlocks | OutputBlocks;

    export enum CommandBlocks {
        COM = 0,
        HAT = 1,
        END = 2,
    }

    export enum OutputBlocks {
        STRING = 0,
        BOOL = 1,
    }

    export enum InputTypes {
        STRING = 0,
        NUMBER = 1,
        BOOL = 2,
        VARIABLE = 3,
        BLOCK = 4,
    }

    export interface InputStyle {
        fill: string;
        stroke: string;
        stroke_width: number;
        radius: number;
        padding: { x: number; y: number };
        font: string;
        text_fill: string;
    }

    export interface BlockLayout {
        padding: { x: number; y: number };
        spacing: number;
        input_spacing: number;
        alignment: "left" | "center" | "right";
    }

    export interface BlockInput {
        type: InputTypes;
        content: string | number | boolean;
        width?: number;
    }

    export interface BlockSize {
        width: number;
        height: number;
    }

    export interface BlockGeometry {
        radius: number;
        notch: {
            width: number;
            depth: number;
            offset?: number;
        };
        top_notch: boolean;
        bottom_connector: boolean;
    }

    export interface BlockConfig {
        geometry?: BlockGeometry;
        fill: string;
        stroke?: string;
        stroke_width?: number;
        text_fill?: string;
        font: string;
        input_style?: InputStyle;
        layout?: BlockLayout;
    }

    export class Block {
        public element: SVGSVGElement | null;
        public inputs: BlockInput[];
        public label: string;

        private measurement_svg: SVGSVGElement | null = null;
        private measurement_text: SVGTextElement | null = null;
        private readonly namespace = "http://www.w3.org/2000/svg";

        private readonly default_layout: BlockLayout = {
            padding: { x: 12, y: 8 },
            spacing: 4,
            input_spacing: 8,
            alignment: "left",
        };

        constructor(private params: BlockConfig, private type: OutputBlockTypes, private text: string) {
            this.element = null;
            this.label = text.trim();
            this.inputs = [];
        }

        private get default_input_style(): InputStyle {
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

        private get layout(): BlockLayout {
            return this.params.layout ?? this.default_layout;
        }

        private get input_style(): InputStyle {
            return this.params.input_style ?? this.default_input_style;
        }

        private get_measurement_elements(): SVGTextElement {
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

        private get_font_size(font: string): number {
            return Number(font.match(/(\d+(?:\.\d+)?)px/)?.[1] ?? 12);
        }

        private apply_segment_style(element: SVGElement, style: { bold: boolean; italic: boolean; decoration: string; font_family: string | null }): void {
            if (style.bold) element.style.fontWeight = "bold";
            if (style.italic) element.style.fontStyle = "italic";
            if (style.decoration) element.style.textDecoration = style.decoration;
            if (style.font_family) element.style.fontFamily = style.font_family;
        }

        private get_text_segments(source: string): Array<{
            content: string;
            style: { bold: boolean; italic: boolean; decoration: string; font_family: string | null };
        }> {
            const parsed = new DOMParser().parseFromString(source, "text/html");
            const segments: Array<{
                content: string;
                style: { bold: boolean; italic: boolean; decoration: string; font_family: string | null };
            }> = [];

            const append_nodes = (
                parent: Node,
                inherited: { bold: boolean; italic: boolean; decoration: string; font_family: string | null },
            ): void => {
                for (const node of Array.from(parent.childNodes)) {
                    if (node.nodeType === Node.TEXT_NODE) {
                        const content = node.textContent ?? "";

                        if (content.length > 0) {
                            segments.push({ content, style: { ...inherited } });
                        }

                        continue;
                    }

                    if (node.nodeType !== Node.ELEMENT_NODE) continue;

                    const element = node as Element;
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

        private measure_segment(content: string, style: { bold: boolean; italic: boolean; decoration: string; font_family: string | null }): number {
            const text = this.get_measurement_elements();

            text.replaceChildren();
            text.style.font = this.params.font;

            const tspan = document.createElementNS(this.namespace, "tspan");
            this.apply_segment_style(tspan, style);
            tspan.textContent = content;

            text.appendChild(tspan);

            return text.getComputedTextLength();
        }

        protected measure_text(content: string, font: string = this.params.font): number {
            const text = this.get_measurement_elements();

            text.replaceChildren();
            text.style.font = font;

            const tspan = document.createElementNS(this.namespace, "tspan");
            tspan.textContent = content;

            text.appendChild(tspan);

            return text.getComputedTextLength();
        }

        private get_segment_width(segment: ReturnType<Block["get_text_segments"]>[number]): number {
            const style = this.input_style;
            return this.measure_segment(segment.content, segment.style) + style.padding.x * 2;
        }

        private get_input_display_value(input: BlockInput): string {
            return String(input.content);
        }

        private validate_input(input: BlockInput): void {
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

        private refresh_input_layout(input: HTMLInputElement): void {
            const previous = this.element;
            if (!previous) return;

            const parent = previous.parentNode;
            if (!parent) return;

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

            const replacement = updated.querySelector<HTMLInputElement>(`input[data-input-index="${index}"]`);

            if (replacement) {
                replacement.focus();

                if (selectionStart !== null && selectionEnd !== null && replacement.type !== "checkbox") {
                    replacement.setSelectionRange(selectionStart, selectionEnd, selectionDirection ?? undefined);
                }
            }
        }

        public calculate_stroke_color(fill: string): string {
            const color = fill.match(/^#([0-9a-f]{6})$/i);

            if (!color) return fill;

            const hex = color[1];
            const channels = [0, 2, 4].map(index => parseInt(hex.substring(index, index + 2), 16));
            const darker = channels.map(channel => Math.round(channel * 0.7));

            return `#${darker.map(channel => channel.toString(16).padStart(2, "0")).join("")}`;
        }

        public add_param(input: BlockInput): this {
            this.validate_input(input);
            this.inputs.push({ ...input });

            if (this.element) {
                const previous = this.element;
                const parent = previous.parentNode;

                this.element = null;

                const updated = this.invoke();

                if (parent) {
                    parent.replaceChild(updated, previous);
                } else {
                    previous.remove();
                }
            }

            return this;
        }

        private get_input_width(input: BlockInput): number {
            if (input.type === InputTypes.BOOL) {
                return Math.max(input.width ?? 24, 24);
            }

            const style = this.input_style;
            const contentWidth = this.measure_text(this.get_input_display_value(input), style.font);
            const automaticWidth = contentWidth + style.padding.x * 2 + 4;

            return Math.max(input.width ?? 0, automaticWidth, 40);
        }

        public calculate_block_size(): BlockSize {
            const layout = this.layout;
            const style = this.input_style;
            const segments = this.get_text_segments(this.label);

            const segmentWidths = segments.map(segment => this.get_segment_width(segment));
            const inputWidths = this.inputs.map(input => this.get_input_width(input));

            const labelSpacing = Math.max(0, segments.length - 1) * layout.spacing;
            const inputSpacing = Math.max(0, inputWidths.length - 1) * layout.input_spacing;
            const labelInputSpacing = segments.length > 0 && inputWidths.length > 0 ? layout.input_spacing : 0;

            const contentWidth = segmentWidths.reduce((total, width) => total + width, 0)
                + inputWidths.reduce((total, width) => total + width, 0)
                + labelSpacing + inputSpacing + labelInputSpacing;

            const segmentHeight = this.get_font_size(this.params.font) + style.padding.y * 2;
            const inputHeight = this.get_font_size(style.font) + style.padding.y * 2;

            return {
                width: Math.max(48, contentWidth + layout.padding.x * 2),
                height: Math.max(48, segmentHeight + layout.padding.y * 2, inputHeight + layout.padding.y * 2),
            };
        }

        private get_notch_layout(size: BlockSize, strokeWidth: number): { start: number; end: number; radius: number; enabled: boolean } {
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
        
        private construct_block_contents(size: BlockSize, strokeWidth: number): string {
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
            const commands: string[] = [];

            commands.push(`M 0 ${outerRadius}`);
            commands.push(`L 0 ${height - outerRadius}`);
            commands.push(`Q 0 ${height} ${outerRadius} ${height}`);

            if (geometry.bottom_connector && notch.enabled) {
                const { start, end, radius } = notch;

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

        private create_input_element(input: BlockInput, x: number, y: number, index: number): SVGGElement {
            const style = this.input_style;
            const group = document.createElementNS(this.namespace, "g");
            const foreignObject = document.createElementNS(this.namespace, "foreignObject");

            const width = this.get_input_width(input);
            const height = this.get_font_size(style.font) + style.padding.y * 2;

            const container = document.createElement("div");
            const element = document.createElement("input");

            container.style.width = `${width}px`;
            container.style.height = `${height}px`;
            container.style.boxSizing = "border-box";
            container.style.display = "flex";
            container.style.alignItems = "center";

            switch (input.type) {
                case InputTypes.NUMBER:
                    element.type = "number";
                    element.step = "any";
                    element.value = String(input.content);
                    break;

                case InputTypes.BOOL:
                    element.type = "checkbox";
                    element.checked = input.content as boolean;
                    break;

                default:
                    element.type = "text";
                    element.value = input.content as string;
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
            } else {
                element.style.width = "100%";
                element.style.height = "100%";
                element.style.padding = `${style.padding.y}px ${style.padding.x}px`;
                element.style.minWidth = "0";
            }

            element.addEventListener("input", () => {
                if (input.type === InputTypes.NUMBER) {
                    if (element.value === "") return;

                    const value = element.valueAsNumber;
                    if (Number.isFinite(value)) input.content = value;
                } else if (input.type !== InputTypes.BOOL) {
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

        private create_label_element(segment: ReturnType<Block["get_text_segments"]>[number], x: number, y: number): SVGGElement {
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

        private disable_text_selection(): void {
            const element = this.element;
            if (!element) return;

            element.style.userSelect = "none";
            element.style.webkitUserSelect = "none";

            element.addEventListener("focusin", (event: FocusEvent) => {
                const target = event.target;

                if (target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement) {
                    target.style.userSelect = "text";
                    target.style.webkitUserSelect = "text";
                }
            });

            element.addEventListener("focusout", (event: FocusEvent) => {
                const target = event.target;

                if (target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement) {
                    target.style.userSelect = "none";
                    target.style.webkitUserSelect = "none";
                }
            });
        }

        public invoke(): SVGSVGElement {
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
            const inputWidths = this.inputs.map(input => this.get_input_width(input));

            const labelSpacing = Math.max(0, segments.length - 1) * layout.spacing;
            const inputSpacing = Math.max(0, inputWidths.length - 1) * layout.input_spacing;
            const labelInputSpacing = segments.length > 0 && inputWidths.length > 0 ? layout.input_spacing : 0;

            const contentWidth = segmentWidths.reduce((total, width) => total + width, 0)
                + (inputWidths?.reduce((total, width) => total || 0 + (width || 0), 0) ?? 0)
                + labelSpacing + inputSpacing + labelInputSpacing;

            let x = layout.padding.x;

            if (layout.alignment === "center") {
                x = (size.width - contentWidth) / 2;
            } else if (layout.alignment === "right") {
                x = size.width - layout.padding.x - contentWidth;
            }

            const labelHeight = this.get_font_size(this.params.font) + style.padding.y * 2;
            const inputHeight = this.get_font_size(style.font) + style.padding.y * 2;
            const contentHeight = Math.max(labelHeight, inputHeight);
            const contentY = (size.height - contentHeight) / 2;

            for (let index = 0; index < segments.length; index++) {
                const segment = segments[index];

                svg.appendChild(this.create_label_element(segment, x + strokeOffset, contentY + strokeOffset));
                x += segmentWidths[index];

                if (index < segments.length - 1) {
                    x += layout.spacing;
                } else if (this.inputs.length > 0) {
                    x += layout.input_spacing;
                }
            }

            for (let index = 0; index < this.inputs.length; index++) {
                const input = this.inputs[index];
                svg.appendChild(this.create_input_element(input, x + strokeOffset, contentY + strokeOffset, index));
                x += inputWidths[index]; if (index < this.inputs.length - 1) { x += layout.input_spacing; }
            }

            svg.classList.add("block-vector-graphics");
            svg.dataset.blockType = String(this.type);

            svg.setAttribute("viewBox", `0 0 ${totalWidth} ${totalHeight}`);
            svg.setAttribute("width", String(totalWidth));
            svg.setAttribute("height", String(totalHeight));

            svg.insertBefore(path, svg.firstChild);

            this.element?.remove();
            this.element = svg;

            this.disable_text_selection();
            this.apply_drag();

            return svg;
        }

        public destroy(): void {
            this.element?.remove();
            this.measurement_svg?.remove();

            this.measurement_svg = null;
            this.measurement_text = null;
            this.element = null;
        }

        public apply_drag(): void {
            const element = this.element;
            if (!element) return;

            let dragging = false;
            let parent: HTMLElement | null = null;
            let startX = 0;
            let startY = 0;
            let initialLeft = 0;
            let initialTop = 0;

            element.style.touchAction = "none";
            element.style.cursor = "grab";

            element.addEventListener("pointerdown", (event: PointerEvent) => {
                if (event.button !== 0 || dragging) return;

                if (event.target instanceof Element && event.target.closest("foreignObject")) return;

                parent = element.parentElement;
                if (!parent) return;

                const elementRect = element.getBoundingClientRect();
                const parentRect = parent.getBoundingClientRect();

                if (getComputedStyle(parent).position === "static") {
                    parent.style.position = "relative";
                }

                startX = event.clientX;
                startY = event.clientY;

                initialLeft = elementRect.left - parentRect.left - parent.clientLeft + parent.scrollLeft;
                initialTop = elementRect.top - parentRect.top - parent.clientTop + parent.scrollTop;

                element.style.position = "absolute";
                element.style.left = `${initialLeft}px`;
                element.style.top = `${initialTop}px`;

                dragging = true;
                element.style.cursor = "grabbing";
                element.style.zIndex = "1000";

                element.setPointerCapture(event.pointerId);
                event.preventDefault();
            });

            element.addEventListener("pointermove", (event: PointerEvent) => {
                if (!dragging) return;

                element.style.left = `${initialLeft + event.clientX - startX}px`;
                element.style.top = `${initialTop + event.clientY - startY}px`;

                event.preventDefault();
            });

            const stopDragging = (event: PointerEvent): void => {
                if (!dragging) return;

                dragging = false;
                element.style.cursor = "grab";
                element.style.zIndex = "";

                if (element.hasPointerCapture(event.pointerId)) {
                    element.releasePointerCapture(event.pointerId);
                }

                parent = null;
            };

            element.addEventListener("pointerup", stopDragging);
            element.addEventListener("pointercancel", stopDragging);
        }
    }
}
