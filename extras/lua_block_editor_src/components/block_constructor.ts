import { VirtualMachine } from './blbvm.js';

export namespace BlockBuilderMachine {
    export const VM = new VirtualMachine.Machine({});
    export type OutputBlockTypes = CommandBlocks | OutputBlocks;
    export let CreatedBlocks: Block[] = [];

    export enum CommandBlocks {
        COM = 0,
        HAT = 1,
        END = 2,
        /** Expression/reporter-shaped block with value inputs and no stack notches. */
        REP = 3,
    }

    // Keep output/value IDs distinct from command block IDs. Both enums are
    // passed as numbers at runtime, so overlapping values make type checks ambiguous.
    export enum OutputBlocks {
        STRING = 4,
        BOOL = 5,
    }

    export enum InputTypes {
        STRING = 0,
        NUMBER = 1,
        BOOL = 2,
        VARIABLE = 3,
        BLOCK = 4,
    }

    export interface ConnectorDefinition {
        connection: Block,
        activated: boolean,
        compiled: boolean,
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

    /**
     * The inline styles that place a block, either in the workspace or inside a
     * BLOCK socket. Every rebuild has to restore all of them: a socket sets
     * display/flex/margin on its child, and a workspace block carries its own
     * position/z-index.
     */
    interface BlockVisualState {
        position: string;
        left: string;
        top: string;
        right: string;
        bottom: string;
        zIndex: string;
        transform: string;
        display: string;
        flex: string;
        margin: string;
        cursor: string;
    }

    /** One rendered piece of a block body: a label chip or a value input. */
    type BlockRowItem =
        | { kind: "label"; segment: { content: string; style: { bold: boolean; italic: boolean; decoration: string; font_family: string | null } } }
        | { kind: "input"; input: BlockInput; index: number };

    export interface BlockNotch {
        side?: "top" | "bottom";
        position: {
            x: number;
            y: number;
        };
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
        font: string;
        /**
         * Toolbox prototype. Template blocks stay in place; pointer-down creates
         * a fresh, editable instance and immediately starts dragging that copy.
         */
        template?: boolean;
        stroke?: string;
        stroke_width?: number;
        text_fill?: string;
        lua_code?: string;
        input_style?: InputStyle;
        layout?: BlockLayout;
        op_code?: Function;
    }

    export class Block {
        public connecting: boolean;
        public block_notches: BlockNotch[];
        public element: SVGSVGElement | null;
        public inputs: BlockInput[];
        public label: string;
        public id: string;

        // A block can have one block connected beneath it. The matching
        // parent reference lets any block find and move the whole stack.
        private connections: ConnectorDefinition[];
        private parent_connection: Block | null = null;
        private readonly input_connections = new Map<number, Block>();
        private parent_input: { block: Block; index: number } | null = null;
        /** True only for editable instances spawned from toolbox templates. */
        private is_toolbox_clone = false;
        /**
         * Corner radius to draw with while this reporter is filling a value input.
         * Null means "draw with the block's own geometry".
         */
        private embed_radius: number | null = null;
        private measurement_svg: SVGSVGElement | null = null;
        private measurement_text: SVGTextElement | null = null;
        private drag_preview_layer: HTMLDivElement | null = null;
        private readonly namespace = "http://www.w3.org/2000/svg";

        private readonly default_layout: BlockLayout = {
            padding: { x: 12, y: 8 },
            spacing: 4,
            input_spacing: 8,
            alignment: "left",
        };

        constructor(
            private params: BlockConfig,
            public type: OutputBlockTypes,
            text: string
        ) {
            this.element = null;
            this.connecting = false;
            this.label = text.trim();
            this.id = this.initialize_unique();
            this.connections = [];
            this.block_notches = [];
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

        private initialize_unique(): string {
            return crypto.randomUUID();
        }

        /** A copy of the current visual/configuration state, not a shared config object. */
        private copy_config(template = false): BlockConfig {
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

        /**
         * Clone the block's current contents, including any reporters already
         * nested inside BLOCK inputs. The clone is always a normal draggable block.
         */
        private clone_current_contents(): Block {
            const clone = new Block(this.copy_config(false), this.type, this.label);
            clone.is_toolbox_clone = true;
            clone.inputs = this.inputs.map((input: BlockInput) => ({ ...input }));

            for (const [index, child] of this.input_connections) {
                const childClone = child.clone_current_contents();
                clone.input_connections.set(index, childClone);
                childClone.parent_input = { block: clone, index };
            }

            clone.invoke();
            return clone;
        }

        public get is_template(): boolean {
            return this.params.template === true;
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

        /** Everything needed to put a rebuilt SVG back exactly where the old one was. */
        private capture_visual_state(element: SVGSVGElement): BlockVisualState {
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

        private apply_visual_state(element: SVGSVGElement, state: BlockVisualState): void {
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

        /**
         * Re-render after an input edit.
         *
         * `raw_value` is the field's literal text. It is written back verbatim so
         * a half-typed number ("1.", "1.0") survives the rebuild instead of being
         * replaced by its parsed value, which used to make decimal reporter values
         * impossible to type.
         */
        private refresh_input_layout(input: HTMLInputElement, raw_value?: string): void {
            const previous = this.element;
            if (!previous) return;

            const parent = previous.parentNode;
            if (!parent) return;

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

            // Scope the lookup to this block: reporters nested in BLOCK sockets
            // publish their own data-input-index values, so an unscoped query can
            // return the wrong field and push the caret into a child reporter.
            const replacement = this.find_input_element(index);

            if (replacement && raw_value !== undefined && replacement.type !== "checkbox") {
                replacement.value = raw_value;
            }

            // The socket owner re-measures *after* this rebuild, and re-inserting
            // this SVG into its new foreignObject drops focus. Focus therefore has
            // to be restored once the whole containment chain has settled, or the
            // next keystroke goes nowhere.
            this.parent_input?.block.refresh_render_preserving_position();

            if (!had_focus) return;

            const restored = this.find_input_element(index);
            if (!restored) return;

            if (raw_value !== undefined && restored.type !== "checkbox") {
                restored.value = raw_value;
            }

            restored.focus();

            if (restore_selection && selection_start !== null && selection_end !== null) {
                try {
                    restored.setSelectionRange(selection_start, selection_end, selection_direction ?? undefined);
                } catch {
                    // Number inputs reject setSelectionRange in some engines.
                }
            }
        }

        /** Draw with the container's rounding while this reporter fills an input. */
        public set_embed_radius(radius: number | null): void {
            if (this.embed_radius === radius) return;

            this.embed_radius = radius;
            this.rerender_in_place();
        }

        /** Rebuild this block where it already sits, keeping every inline style. */
        private rerender_in_place(): void {
            const previous = this.element;
            if (!previous) return;

            const parent = previous.parentNode;
            const state = this.capture_visual_state(previous);

            this.element = null;
            const updated = this.invoke();
            this.apply_visual_state(updated, state);

            if (parent) {
                parent.replaceChild(updated, previous);
            } else {
                previous.replaceWith(updated);
            }
        }

        /**
         * A detached render used by drag previews: same content, drawn with an
         * optional slot radius, and never inserted or registered.
         */
        public render_detached(radius: number | null = null): SVGSVGElement {
            const live = this.element;
            const previous_radius = this.embed_radius;

            this.element = null;
            this.embed_radius = radius;

            const element = this.invoke();

            this.embed_radius = previous_radius;
            this.element = live;

            return element;
        }

        /** This block's own field, never a field belonging to a nested reporter. */
        private find_input_element(index: string | undefined): HTMLInputElement | null {
            const element = this.element;
            if (!element || index === undefined) return null;

            return element.querySelector<HTMLInputElement>(
                `input[data-block-id="${this.id}"][data-input-index="${index}"]`,
            );
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
                const state = this.capture_visual_state(previous);

                this.element = null;

                const updated = this.invoke();
                this.apply_visual_state(updated, state);

                if (parent) {
                    parent.replaceChild(updated, previous);
                } else {
                    previous.remove();
                }

                this.parent_input?.block.refresh_render_preserving_position();
            }

            return this;
        }

        private get_input_width(input: BlockInput, index: number): number {
            // A reporter sitting in this input decides how much room it needs,
            // whatever the input's own type is.
            const connected = this.input_connections.get(index);
            if (connected) {
                return Math.max(input.width ?? 0, connected.get_render_size().width);
            }

            const style = this.input_style;
            const font_size = this.get_font_size(style.font);

            // Every width below is measured, so a field is exactly as wide as its
            // contents instead of being clamped to a fixed pixel minimum.
            let contentWidth: number;
            if (input.type === InputTypes.BOOL) {
                contentWidth = font_size;
            } else if (input.type === InputTypes.BLOCK) {
                contentWidth = this.measure_text(String(input.content || "..."), style.font);
            } else {
                contentWidth = this.measure_text(this.get_input_display_value(input), style.font);
            }

            const automaticWidth = contentWidth + style.padding.x * 2 + style.stroke_width * 2 + 2;

            return Math.max(input.width ?? 0, automaticWidth);
        }

        private get_input_height(input: BlockInput, index: number): number {
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

        private get_render_size(): BlockSize {
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

        /**
         * Lay the block out as one row: every label chip in order, then every value
         * input in input order. Labels are plain text; nothing in them is a marker.
         */
        private get_row_items(): BlockRowItem[] {
            const items: BlockRowItem[] = [];

            for (const segment of this.get_text_segments(this.label)) {
                items.push({ kind: "label", segment });
            }

            for (let index = 0; index < this.inputs.length; index++) {
                items.push({ kind: "input", input: this.inputs[index], index });
            }

            return items;
        }

        private get_row_item_width(item: BlockRowItem): number {
            return item.kind === "label"
                ? this.get_segment_width(item.segment)
                : this.get_input_width(item.input, item.index);
        }

        private get_row_item_height(item: BlockRowItem): number {
            return item.kind === "label"
                ? this.get_font_size(this.params.font) + this.input_style.padding.y * 2
                : this.get_input_height(item.input, item.index);
        }

        private get_row_gap(left: BlockRowItem, right: BlockRowItem): number {
            const layout = this.layout;
            return left.kind === "input" || right.kind === "input"
                ? layout.input_spacing
                : layout.spacing;
        }

        private measure_row(items: BlockRowItem[]): number {
            let width = 0;

            for (let index = 0; index < items.length; index++) {
                width += this.get_row_item_width(items[index]);
                if (index > 0) width += this.get_row_gap(items[index - 1], items[index]);
            }

            return width;
        }

        public calculate_block_size(): BlockSize {
            const layout = this.layout;
            const style = this.input_style;
            const geometry = this.get_geometry();
            const items = this.get_row_items();
            const contentWidth = this.measure_row(items);

            // The body is driven by the row it holds, not a fixed 48px floor. The
            // only remaining minimum is the one the geometry itself needs: room
            // for both rounded ends and, when the block connects, the notch.
            const has_connectors = geometry.bottom_connector || geometry.top_notch;
            const notch_start = geometry.notch.offset ?? geometry.radius * 2;
            const minimumWidth = has_connectors
                ? notch_start + geometry.notch.width * 2 + geometry.radius
                : geometry.radius * 2;
            const minimumHeight = geometry.radius * 2 +
                (geometry.bottom_connector ? geometry.notch.depth : 0);

            const textHeight = this.get_font_size(this.params.font) + style.padding.y * 2;
            const rowHeight = Math.max(textHeight, items.reduce(
                (maximum, item) => Math.max(maximum, this.get_row_item_height(item)), 0,
            ));

            return {
                width: Math.max(minimumWidth, contentWidth + layout.padding.x * 2),
                height: Math.max(minimumHeight, rowHeight + layout.padding.y * 2),
            };
        }

        private get_notch_layout(size: BlockSize, strokeWidth: number): { start: number; end: number; radius: number; enabled: boolean } {
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

        private construct_block_contents(size: BlockSize, strokeWidth: number): string {
            const geometry = this.get_geometry();

            const { width, height } = size;
            const { depth } = geometry.notch;
            const outerRadius = Math.min(geometry.radius, height / 2, width / 2);
            const notch = this.get_notch_layout(size, strokeWidth);
            const commands: string[] = [];

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

        private create_input_element(input: BlockInput, x: number, y: number, index: number): SVGGElement {
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

            // Every value input is a drop target, not just BLOCK sockets: a
            // reporter dropped on one of these overrides the literal contents.
            container.className = "block-input-slot";
            container.dataset.blockId = this.id;
            container.dataset.blockInputIndex = String(index);
            container.dataset.inputType = InputTypes[input.type].toLowerCase();

            const connected = this.input_connections.get(index);

            if (connected) {
                // Draw the reporter with this input's rounding so it reads as a
                // value in the row instead of a block wedged into a box.
                connected.set_embed_radius(style.radius + 3);
            }

            const occupant = connected?.element ?? null;

            if (occupant) {
                // A reporter owns this input. Show it instead of the literal field;
                // input.content keeps the old value so removing the reporter
                // restores what the user had typed.
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
                // Empty socket: keep the dashed placeholder box.
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

            // The public hook for host stylesheets (e.g. `user-select: all`).
            element.classList.add("block-input");

            element.dataset.inputType = InputTypes[input.type].toLowerCase();
            element.dataset.inputIndex = String(index);
            // Identifies the owning block so post-edit lookups cannot reach into
            // a reporter nested in one of this block's sockets.
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
            // The block itself shows a grab cursor; a value field must not.
            element.style.cursor = input.type === InputTypes.BOOL ? "pointer" : "text";

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

                // Only rebuild when the field actually needs a different width.
                // Rebuilding on every keystroke re-rendered the field from the
                // parsed value, so "1." collapsed back to "1" and the caret was
                // reset; that is what broke decimal reporter values.
                const rendered_width = Number.parseFloat(element.parentElement?.style.width ?? "");
                const required_width = this.get_input_width(input, index);

                if (!(Number.isFinite(rendered_width) && Math.abs(rendered_width - required_width) < 0.5)) {
                    this.refresh_input_layout(element, element.value);
                }

                // Typing changes the block's own size, so the content readout has to
                // follow instead of waiting for the next drag to re-measure it.
                BlockWorkspaceEditor.refresh_content_bounds();
            });

            element.addEventListener("change", () => {
                if (input.type === InputTypes.BOOL) {
                    input.content = element.checked;
                }

                BlockWorkspaceEditor.refresh_content_bounds();
            });

            // Leaving the field settles the edit, so the readout is refreshed here
            // too: a value can change width without changing the field's box.
            element.addEventListener("blur", () => {
                BlockWorkspaceEditor.refresh_content_bounds();
            });

            container.appendChild(element);

            return this.assemble_input_group(group, foreignObject, container, input, x, y, width, height);
        }

        private assemble_input_group(
            group: SVGGElement,
            foreignObject: SVGForeignObjectElement,
            container: HTMLDivElement,
            input: BlockInput,
            x: number,
            y: number,
            width: number,
            height: number,
        ): SVGGElement {
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
            } else if (layout.alignment === "right") {
                x = size.width - layout.padding.x - contentWidth;
            }

            const labelHeight = this.get_font_size(this.params.font) + style.padding.y * 2;
            // BLOCK inputs can contain reporters taller than the surrounding text.
            // Use their real rendered heights when vertically laying out the row;
            // calculate_block_size() already uses these same dimensions for the body.
            const contentHeight = items.reduce(
                (maximum, item) => Math.max(maximum, this.get_row_item_height(item)),
                labelHeight,
            );
            const contentY = (size.height - contentHeight) / 2;

            for (let index = 0; index < items.length; index++) {
                const item = items[index];

                // Centre each item against the tallest thing in the row, so a label
                // chip lines up with a reporter that is taller than the text.
                const itemHeight = this.get_row_item_height(item);
                const itemY = contentY + (contentHeight - itemHeight) / 2;

                if (item.kind === "label") {
                    svg.appendChild(this.create_label_element(item.segment, x + strokeOffset, itemY + strokeOffset));
                } else {
                    svg.appendChild(this.create_input_element(item.input, x + strokeOffset, itemY + strokeOffset, item.index));
                }

                x += this.get_row_item_width(item);
                if (index < items.length - 1) x += this.get_row_gap(item, items[index + 1]);
            }

            svg.classList.add("block-vector-graphics");
            svg.dataset.blockType = String(this.type);
            svg.dataset.blockId = this.id;
            if (this.is_template) {
                svg.dataset.blockTemplate = "true";
                svg.setAttribute("aria-label", `${this.label || "Block"} toolbox template`);
            } else {
                delete svg.dataset.blockTemplate;
            }

            svg.setAttribute("viewBox", `0 0 ${totalWidth} ${totalHeight}`);
            svg.setAttribute("width", String(totalWidth));
            svg.setAttribute("height", String(totalHeight));
            svg.setAttribute("id", this.id);

            svg.insertBefore(path, svg.firstChild);

            this.element?.remove();
            this.element = svg;

            // invoke() can run again when an input changes, so do not register
            // the same Block more than once in the workspace.
            if (!BlockBuilderMachine.CreatedBlocks.includes(this)) {
                BlockBuilderMachine.CreatedBlocks.push(this);
            }

            this.disable_text_selection();
            this.apply_drag();

            return svg;
        }

        public destroy(): void {
            // Drop any drag preview this block owns; it lives outside the block's
            // own subtree, so nothing else would ever remove it.
            this.clear_stack_preview();

            // Hand the socket children over before tearing anything down. Clearing
            // the map first stops each child's destroy() from rebuilding this block
            // (and re-registering it in CreatedBlocks) once per reporter.
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
            BlockBuilderMachine.CreatedBlocks = BlockBuilderMachine.CreatedBlocks.filter(
                (block: Block) => block !== this,
            );

            this.element?.remove();
            this.measurement_svg?.remove();

            this.measurement_svg = null;
            this.measurement_text = null;
            this.element = null;
            this.block_notches = [];
        }

        private get_geometry(): BlockGeometry {
            const geometry = this.params.geometry ?? {
                radius: 5,
                notch: { width: 14, depth: 8 },
                top_notch: false,
                bottom_connector: false,
            };

            if (this.embed_radius === null) return geometry;

            // Filling a value input: no stack notches, and the input's rounding so
            // the reporter reads as a value instead of a block stuffed in a box.
            return {
                ...geometry,
                radius: this.embed_radius,
                top_notch: false,
                bottom_connector: false,
            };
        }

        private has_top_notch(): boolean {
            return this.get_geometry().top_notch && this.get_notch_layout(
                this.calculate_block_size(),
                this.params.stroke_width ?? 1.5,
            ).enabled;
        }

        private has_bottom_connector(): boolean {
            return this.get_geometry().bottom_connector && this.get_notch_layout(
                this.calculate_block_size(),
                this.params.stroke_width ?? 1.5,
            ).enabled;
        }

        private get_next_block(): Block | null {
            return this.connections.find((connection: ConnectorDefinition) => connection.activated)?.connection ?? null;
        }

        private get_stack_root(): Block {
            let current: Block = this;
            const visited = new Set<Block>();

            while (current.parent_connection && !visited.has(current.parent_connection)) {
                visited.add(current);
                current = current.parent_connection;
            }

            return current;
        }

        /** Return the tail of the chain beginning at this block (not its ancestor root). */
        private get_stack_tail(): Block {
            let current: Block = this;
            const visited = new Set<Block>();

            while (current.get_next_block() && !visited.has(current.get_next_block()!)) {
                visited.add(current);
                current = current.get_next_block()!;
            }

            return current;
        }

        /** Return this block and everything connected beneath it, preserving chain order. */
        private get_stack_members(): Block[] {
            const members: Block[] = [];
            const visited = new Set<Block>();
            let current: Block | null = this;

            while (current && !visited.has(current)) {
                visited.add(current);
                members.push(current);
                current = current.get_next_block();
            }

            return members;
        }

        /**
         * Everything that travels with this block when it is dragged: the stack
         * below it, plus every reporter nested in a BLOCK socket, in parent-first
         * order so callers can resolve containment in a single pass.
         */
        private collect_segment_blocks(): Block[] {
            const blocks: Block[] = [];
            const seen = new Set<Block>();

            const visit = (block: Block): void => {
                if (seen.has(block)) return;
                seen.add(block);
                blocks.push(block);

                for (const child of block.input_connections.values()) visit(child);
            };

            for (const block of this.get_stack_members()) visit(block);

            return blocks;
        }

        private is_reporter(): boolean {
            return this.type === CommandBlocks.REP ||
                this.type === OutputBlocks.STRING ||
                this.type === OutputBlocks.BOOL;
        }

        private can_connect_to(lower_block: Block): boolean {
            if (this === lower_block || this.is_reporter() || lower_block.is_reporter()) return false;
            if (!this.has_bottom_connector() || !lower_block.has_top_notch()) return false;
            if (this.get_next_block() !== null || lower_block.parent_connection !== null) return false;
            if (this.get_stack_root() === lower_block.get_stack_root()) return false;

            return true;
        }

        /** Connect this command block underneath `this` (this is the upper block). */
        public insert_connectors(lower_block: Block): ConnectorDefinition | undefined {
            if (!this.can_connect_to(lower_block)) return undefined;

            const connector: ConnectorDefinition = {
                connection: lower_block,
                activated: true,
                compiled: false,
            };

            this.connections.push(connector);
            lower_block.parent_connection = this;

            return connector;
        }

        /** Remove a specific link, or detach this block from both sides. */
        public remove_connectors(connection?: Block): void {
            const outgoing = this.connections.filter((item: ConnectorDefinition) =>
                connection ? item.connection === connection : true,
            );

            for (const item of outgoing) {
                if (item.connection.parent_connection === this) {
                    item.connection.parent_connection = null;
                }
            }

            this.connections = this.connections.filter((item: ConnectorDefinition) =>
                connection ? item.connection !== connection : false,
            );

            if (!connection || this.parent_connection === connection) {
                const parent = this.parent_connection;

                if (parent) {
                    parent.connections = parent.connections.filter(
                        (item: ConnectorDefinition) => item.connection !== this,
                    );
                }

                this.parent_connection = null;
            }
        }

        /**
         * Resolve the connector's real SVG anchor into viewport CSS pixels.
         * Do not reconstruct this from the element bounding box: SVG viewBox
         * scaling, CSS sizing, stroke offsets and transforms all affect that.
         */
        private get_connector_world_point(side: "top" | "bottom"): { x: number; y: number } | null {
            const element = this.element;
            if (!element?.isConnected) return null;

            const notch = this.block_notches.find((item: BlockNotch) => item.side === side);
            if (!notch) return null;

            const matrix = element.getScreenCTM();
            if (!matrix) return null;

            const strokeOffset = (this.params.stroke_width ?? 1.5) / 2;
            const point = element.createSVGPoint();
            // block_notches are recorded in path-local coordinates; the path
            // itself is translated by strokeOffset in invoke().
            point.x = notch.position.x + strokeOffset;
            point.y = notch.position.y + strokeOffset;

            const screenPoint = point.matrixTransform(matrix);
            return { x: screenPoint.x, y: screenPoint.y };
        }

        private get_connector_world_x(): number {
            return this.get_connector_world_point(this.has_top_notch() ? "top" : "bottom")?.x ?? 0;
        }

        private get_body_top_world_y(): number {
            const element = this.element;
            if (!element) return 0;

            return element.getBoundingClientRect().top + (this.params.stroke_width ?? 1.5) / 2;
        }

        private get_body_bottom_world_y(): number {
            return this.get_body_top_world_y() + this.calculate_block_size().height;
        }


        private move_blocks_by(blocks: Block[], delta_x: number, delta_y: number): void {
            for (const block of blocks) {
                const element = block.element;
                const parent = element?.parentElement;
                if (!element || !parent) continue;

                if (getComputedStyle(parent).position === "static") {
                    parent.style.position = "relative";
                }

                const scale = BlockWorkspaceEditor.get_workspace_scale();
                const rect = element.getBoundingClientRect();
                const parentRect = parent.getBoundingClientRect();

                // delta_x/delta_y arrive in screen pixels (connector anchors and
                // measured heights), so they are divided back into canvas units.
                const left = (rect.left + delta_x - parentRect.left) / scale - parent.clientLeft + parent.scrollLeft;
                const top = (rect.top + delta_y - parentRect.top) / scale - parent.clientTop + parent.scrollTop;

                element.style.position = "absolute";
                element.style.left = `${left}px`;
                element.style.top = `${top}px`;
            }
        }

        private get_workspace_root_block(): Block {
            let current: Block = this;
            const visited = new Set<Block>();

            while (current.parent_input && !visited.has(current.parent_input.block)) {
                visited.add(current.parent_input.block);
                current = current.parent_input.block;
            }

            return current.get_stack_root();
        }

        /**
         * Rebuild this block after a reporter is inserted/removed, preserving its
         * workspace position. Rebuilding a nested reporter also refreshes every
         * containing reporter and ultimately the command block that owns the socket.
         *
         * Width and height are recalculated by calculate_block_size() from the
         * connected reporter's get_render_size(), so the socket and its owner grow
         * to fit the imported reporter instead of clipping or overlapping it.
         */
        private refresh_render_preserving_position(visited: Set<Block> = new Set()): void {
            if (visited.has(this)) return;
            visited.add(this);

            const previous = this.element;
            if (!previous) return;

            const parent = previous.parentNode;
            const state = this.capture_visual_state(previous);
            const previous_height = previous.getBoundingClientRect().height;

            // invoke() recalculates the outer SVG viewBox, body path, and BLOCK
            // foreignObject dimensions using the newly connected reporter.
            this.element = null;
            const updated = this.invoke();
            this.apply_visual_state(updated, state);

            if (parent) {
                parent.replaceChild(updated, previous);
            } else {
                previous.replaceWith(updated);
            }

            // A taller block (a reporter just filled its socket) pushes the rest
            // of its stack down. Without this the blocks below stay put and the
            // stack visually breaks apart.
            const height_delta = updated.getBoundingClientRect().height - previous_height;
            if (Math.abs(height_delta) > 0.5) {
                const downstream = this.get_stack_members().slice(1);
                if (downstream.length > 0) this.move_blocks_by(downstream, 0, height_delta);
            }

            // If this block is itself a reporter inside another BLOCK input,
            // propagate its new dimensions all the way up the containment chain.
            this.parent_input?.block.refresh_render_preserving_position(visited);
        }

        /** Detach a reporter from its current socket so it can be dragged independently. */
        private detach_from_input(workspace_override?: HTMLElement | null): void {
            const parentInput = this.parent_input;
            if (!parentInput) return;

            // Back to its own geometry before it becomes a free workspace block.
            this.set_embed_radius(null);

            const element = this.element;
            if (!element) return;

            // The socket nests this SVG inside a foreignObject, so the containing
            // block's parent element is not necessarily the editor canvas.
            const workspace = workspace_override
                ?? BlockWorkspaceEditor.get_workspace(element)
                ?? parentInput.block.get_workspace_root_block().element?.parentElement
                ?? null;
            if (!workspace) return;

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
            // Undo the socket-side styling create_input_element() applied.
            element.style.right = "auto";
            element.style.bottom = "auto";
            element.style.zIndex = "";
            element.style.display = "";
            element.style.flex = "";
            element.style.margin = "";
            element.style.cursor = "grab";

            parentInput.block.refresh_render_preserving_position();
        }

        private has_input_descendant(candidate: Block, visited: Set<Block> = new Set()): boolean {
            if (this === candidate) return true;
            if (visited.has(this)) return false;
            visited.add(this);

            for (const child of this.input_connections.values()) {
                if (child === candidate || child.has_input_descendant(candidate, visited)) {
                    return true;
                }
            }

            return false;
        }

        /** Find the nearest real BLOCK socket using the rendered HTML socket nodes.
         * The DOM is authoritative here: SVG foreignObject descendants are not
         * reliably discoverable by querying an SVG root in every browser engine.
         */
        private find_reporter_input_drop_plan(
            reporter: Block,
            client_x: number,
            client_y: number,
        ): {
            kind: "reporter-input";
            target: Block;
            index: number;
            slot: HTMLElement;
            rect: DOMRect;
            delta_x: number;
            delta_y: number;
            score: number;
        } | null {
            if (!reporter.is_reporter() || reporter.parent_input || !reporter.element) return null;

            const reporterRect = reporter.element.getBoundingClientRect();
            const reporterArea = Math.max(1, reporterRect.width * reporterRect.height);
            const candidates: Array<{
                kind: "reporter-input";
                target: Block;
                index: number;
                slot: HTMLElement;
                rect: DOMRect;
                delta_x: number;
                delta_y: number;
                score: number;
            }> = [];

            // Query the document, not target.element.querySelectorAll(): slots are
            // HTML nodes hosted inside SVG foreignObject elements. Every value input
            // is a candidate, not only BLOCK sockets.
            const slots = Array.from(document.querySelectorAll<HTMLElement>(
                '.block-input-slot[data-block-id][data-block-input-index]',
            ));

            for (const slot of slots) {
                const target = BlockBuilderMachine.CreatedBlocks.find(
                    block => block.id === slot.dataset.blockId,
                );
                if (!target || target === reporter || target.is_template || !target.element?.isConnected) continue;
                const index = Number(slot.dataset.blockInputIndex);
                if (!Number.isInteger(index) || !target.inputs[index]) continue;
                // A block anywhere in a stack can take a reporter; the tail below
                // it is shifted when the block grows (see
                // refresh_render_preserving_position).
                if (reporter.has_input_descendant(target)) continue;

                const rect = slot.getBoundingClientRect();
                if (rect.width <= 0 || rect.height <= 0) continue;

                // A generous magnet zone, but the pointer must be near the actual
                // socket OR the dragged reporter must physically overlap it.
                const expandX = Math.max(18, Math.min(48, reporterRect.width * 0.35));
                const expandY = Math.max(18, Math.min(42, reporterRect.height * 0.35));
                const dx = client_x < rect.left ? rect.left - client_x
                    : client_x > rect.right ? client_x - rect.right : 0;
                const dy = client_y < rect.top ? rect.top - client_y
                    : client_y > rect.bottom ? client_y - rect.bottom : 0;
                const pointerDistance = Math.hypot(dx, dy);

                const overlapWidth = Math.max(0,
                    Math.min(reporterRect.right, rect.right) - Math.max(reporterRect.left, rect.left),
                );
                const overlapHeight = Math.max(0,
                    Math.min(reporterRect.bottom, rect.bottom) - Math.max(reporterRect.top, rect.top),
                );
                const overlapArea = overlapWidth * overlapHeight;
                const overlapRatio = overlapArea / Math.max(1, Math.min(reporterArea, rect.width * rect.height));
                const pointerNear = dx <= expandX && dy <= expandY;
                if (!pointerNear && overlapRatio < 0.04) continue;

                // Anchor the reporter's top-left to the socket's top-left. The
                // same delta drives preview and commit, so they cannot diverge.
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

        /** Public convenience method for editor integrations and diagnostics. */
        public accepts_reporter_input_drop(reporter: Block, client_x: number, client_y: number): boolean {
            return this.find_reporter_input_drop_plan(reporter, client_x, client_y) !== null;
        }

        /** Commit the same reporter/socket plan used by the live preview. */
        private try_drop_reporter_into_input(
            reporter: Block,
            plan: NonNullable<ReturnType<Block["find_reporter_input_drop_plan"]>>,
        ): boolean {
            if (!reporter.is_reporter() || reporter.parent_input) return false;

            const { target, index, slot } = plan;
            if (!target.element?.isConnected || !target.inputs[index] || !slot.isConnected) return false;
            const rect = slot.getBoundingClientRect();

            const previous = target.input_connections.get(index);
            if (previous) {
                const workspace = BlockWorkspaceEditor.get_workspace(target.element);
                if (!workspace) return false;

                const oldElement = previous.element;
                if (oldElement) {
                    oldElement.remove();
                    workspace.appendChild(oldElement);
                    oldElement.style.position = "absolute";
                    // Park the swapped-out reporter beside the socket. Reusing the
                    // socket rect put it exactly underneath the incoming reporter.
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

        /** Find the closest legal stack connection without changing the graph. */
        private find_stack_drop_plan(source_root: Block): {
            upper: Block;
            lower: Block;
            delta_x: number;
            delta_y: number;
            distance: number;
        } | null {
            if (source_root.is_reporter() || source_root.is_template) return null;

            const source_blocks = source_root.get_stack_members();
            const source_set = new Set(source_blocks);
            const source_top = source_root;
            const source_bottom = source_root.get_stack_tail();
            const candidates = [...BlockBuilderMachine.CreatedBlocks].reverse();

            let best: {
                upper: Block;
                lower: Block;
                delta_x: number;
                delta_y: number;
                distance: number;
            } | null = null;

            for (const candidate of candidates) {
                if (
                    source_set.has(candidate) || candidate.is_reporter() || candidate.is_template ||
                    !candidate.element?.isConnected
                ) continue;
                if (candidate.parent_input || candidate !== candidate.get_stack_root()) continue;

                const target_root = candidate;
                const target_bottom = target_root.get_stack_tail();

                // Option A: place the dragged stack above the target stack.
                if (source_bottom.can_connect_to(target_root)) {
                    const sourceAnchor = source_bottom.get_connector_world_point("bottom");
                    const targetAnchor = target_root.get_connector_world_point("top");
                    if (!sourceAnchor || !targetAnchor) continue;
                    const delta_x = targetAnchor.x - sourceAnchor.x;
                    const delta_y = targetAnchor.y - sourceAnchor.y;
                    // Blockly-style connection radius: activate only when the
                    // dragged connector anchor is actually near the target anchor.
                    // Once activated, delta_x/delta_y still describe the exact final
                    // alignment; the cursor position never becomes the snap origin.
                    // Distances here are screen pixels, so the magnet is scaled to
                    // stay the same size on the canvas at any zoom.
                    const snapRadius = Math.max(
                        28,
                        (source_bottom.get_geometry().notch.width + target_root.get_geometry().notch.width) * 0.75,
                        (source_bottom.get_geometry().notch.depth + target_root.get_geometry().notch.depth) * 1.5,
                    ) * BlockWorkspaceEditor.get_workspace_scale();
                    const distance = Math.hypot(delta_x, delta_y);

                    if (distance <= snapRadius) {
                        if (!best || distance < best.distance) {
                            best = { upper: source_bottom, lower: target_root, delta_x, delta_y, distance };
                        }
                    }
                }

                // Option B: place the dragged stack below the target stack.
                if (target_bottom.can_connect_to(source_top)) {
                    const targetAnchor = target_bottom.get_connector_world_point("bottom");
                    const sourceAnchor = source_top.get_connector_world_point("top");
                    if (!targetAnchor || !sourceAnchor) continue;
                    const delta_x = targetAnchor.x - sourceAnchor.x;
                    const delta_y = targetAnchor.y - sourceAnchor.y;
                    const snapRadius = Math.max(
                        28,
                        (target_bottom.get_geometry().notch.width + source_top.get_geometry().notch.width) * 0.75,
                        (target_bottom.get_geometry().notch.depth + source_top.get_geometry().notch.depth) * 1.5,
                    ) * BlockWorkspaceEditor.get_workspace_scale();
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

        /** Perform a stack connection immediately (also useful to editor integrations). */
        private try_connect_stack(source_root: Block): boolean {
            // Capture only the dragged segment before linking; after insertion both
            // sides become one stack, and moving the merged members would move the target too.
            const source_blocks = source_root.get_stack_members();
            const plan = source_root.find_stack_drop_plan(source_root);
            if (!plan || !plan.upper.insert_connectors(plan.lower)) return false;

            source_root.move_blocks_by(source_blocks, plan.delta_x, plan.delta_y);
            source_root.clear_stack_preview();
            return true;
        }

        /**
         * Delete a toolbox instance after it has been dropped back on the toolbox.
         *
         * Only clones go away. The dragged segment is a whole stack, and the old
         * code destroyed every block in it, so a hand-made block the user had
         * connected underneath a toolbox clone was deleted along with the clone.
         * Anything that is not a clone, and anything a survivor still owns, is
         * released into the workspace where the user dropped it.
         */
        private return_to_toolbox(): void {
            const workspace = BlockWorkspaceEditor.get_workspace(this.element);
            // Without a canvas there is nowhere to park survivors; deleting
            // nothing is safer than deleting user work.
            if (!workspace) return;

            const segment = this.collect_segment_blocks();
            const removed = new Set<Block>();

            for (const block of segment) {
                if (!block.is_toolbox_clone) continue;

                const container = block.parent_input?.block ?? block.parent_connection;
                if (!container || removed.has(container)) {
                    removed.add(block);
                }
            }

            // Release survivors first, so a doomed container cannot take a
            // hand-made block down with it.
            for (const block of segment) {
                if (removed.has(block)) continue;

                const container = block.parent_input?.block ?? block.parent_connection;
                if (!container || !removed.has(container)) continue;

                if (block.parent_input) block.detach_from_input(workspace);
                if (block.parent_connection) block.parent_connection.remove_connectors(block);
            }

            // Deepest first: a parent's destroy() must never reach a survivor.
            for (const block of [...removed].reverse()) {
                block.destroy();
            }
        }

        /**
         * Universal drag preview. The switch selects the appropriate connection
         * geometry, while the rendering path always clones the real SVG content.
         */
        private render_drop_preview(
            dragged: Block[],
            plan:
                | { kind: "stack"; delta_x: number; delta_y: number }
                | { kind: "reporter-input"; delta_x: number; delta_y: number; target?: Block }
                | { kind: "toolbox-return"; delta_x: number; delta_y: number },
        ): void {
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

                    // Reporter drops preview only the reporter being inserted;
                    // stack drops preview every block in the dragged segment.
                    const previewBlocks = plan.kind === "reporter-input" ? dragged.slice(0, 1) : dragged;

                    // Mimic the drop: the ghost is drawn with the target input's
                    // rounding, so the preview shows what will actually land.
                    const preview_radius = plan.kind === "reporter-input" && plan.target
                        ? plan.target.input_style.radius + 3
                        : null;

                    for (const [index, block] of previewBlocks.entries()) {
                        const original = block.element;
                        if (!original?.isConnected) continue;

                        const rect = original.getBoundingClientRect();
                        const preview = preview_radius === null
                            ? original.cloneNode(true) as SVGSVGElement
                            : block.render_detached(preview_radius);
                        preview.removeAttribute("id");
                        preview.querySelectorAll("[id]").forEach((node: Element) => node.removeAttribute("id"));
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

        private clear_stack_preview(): void {
            this.drag_preview_layer?.remove();
            this.drag_preview_layer = null;
        }

        /**
         * Toolbox templates are immutable prototypes. A pointer-down creates a
         * fresh block from their live contents in the editor workspace, then hands
         * the same pointer gesture directly to that new instance's drag routine.
         */
        private create_template_instance(event: PointerEvent): void {
            const templateElement = this.element;
            if (!templateElement) return;

            const workspace = BlockWorkspaceEditor.get_workspace(templateElement);
            if (!workspace) return;

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

            // The gesture may never become a drag (a click, or a pointer that
            // ended on the toolbox). Do not leave a stray copy behind in that case.
            if (!clone.begin_drag(event)) {
                clone.destroy();
            }
        }

        /** Returns false when the gesture never turned into a drag. */
        private begin_drag(event: PointerEvent): boolean {
            const element = this.element;
            if (!element || event.button !== 0) return false;

            // Ignore editable controls inside ordinary blocks. A reporter already
            // embedded in a BLOCK socket is the exception: it must be detachable,
            // even though its SVG is currently nested inside a foreignObject.
            if (event.target instanceof Element) {
                const editable = event.target.closest("input, textarea, select, button");
                if (editable) return false;

            }

            event.stopPropagation();
            this.detach_from_input();

            // Detach at the exact block the user grabbed. The blocks above stay
            // where they are; this block and every descendant become the dragged
            // segment. This is the same directional behavior as Blockly stacks.
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
                if (!blockElement || !parent) return [];

                if (getComputedStyle(parent).position === "static") {
                    parent.style.position = "relative";
                }

                const rect = blockElement.getBoundingClientRect();
                const parentRect = parent.getBoundingClientRect();
                // Screen pixels → workspace units: the canvas may be zoomed.
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

            if (snapshots.length === 0) return false;

            let dragging = true;
            let moved = false;
            // Listen on window: reporters are reparented out of foreignObject
            // sockets at drag start, and pointer capture can be lost on reparent.
            event.preventDefault();

            const move = (moveEvent: PointerEvent): void => {
                if (!dragging) return;

                const deltaX = moveEvent.clientX - startX;
                const deltaY = moveEvent.clientY - startY;
                if (Math.abs(deltaX) > 2 || Math.abs(deltaY) > 2) moved = true;

                const scale = BlockWorkspaceEditor.get_workspace_scale();
                for (const snapshot of snapshots) {
                    snapshot.element.style.left = `${snapshot.left + deltaX / scale}px`;
                    snapshot.element.style.top = `${snapshot.top + deltaY / scale}px`;
                }

                if (moved) {
                    if (BlockWorkspaceEditor.is_point_in_toolbox(moveEvent.clientX, moveEvent.clientY) && root.is_toolbox_clone) {
                        root.render_drop_preview(root.is_reporter() ? [root] : stack, { kind: "toolbox-return", delta_x: 0, delta_y: 0 });
                    } else if (root.is_reporter()) {
                        const plan = root.find_reporter_input_drop_plan(root, moveEvent.clientX, moveEvent.clientY);
                        if (plan) root.render_drop_preview([root], plan);
                        else root.clear_stack_preview();
                    } else {
                        const plan = root.find_stack_drop_plan(root);
                        if (plan) root.render_drop_preview(stack, { kind: "stack", ...plan });
                        else root.clear_stack_preview();
                    }
                }

                moveEvent.preventDefault();
            };

            const finish = (finishEvent: PointerEvent, shouldConnect: boolean): void => {
                if (!dragging) return;
                dragging = false;

                window.removeEventListener("pointermove", move, true);
                window.removeEventListener("pointerup", onPointerUp, true);
                window.removeEventListener("pointercancel", onPointerCancel, true);

                const over_toolbox = root.is_toolbox_clone &&
                    BlockWorkspaceEditor.is_point_in_toolbox(finishEvent.clientX, finishEvent.clientY);

                if (shouldConnect && moved && over_toolbox) {
                    // Dropped back on the toolbox: delete the clones, keep the rest.
                    root.return_to_toolbox();
                } else if (shouldConnect && moved) {
                    if (root.is_reporter()) {
                        const plan = root.find_reporter_input_drop_plan(root, finishEvent.clientX, finishEvent.clientY);
                        if (plan) root.try_drop_reporter_into_input(root, plan);
                    } else {
                        const plan = root.find_stack_drop_plan(root);
                        if (plan && plan.upper.insert_connectors(plan.lower)) {
                            root.move_blocks_by(stack, plan.delta_x, plan.delta_y);
                        }
                    }
                }

                root.clear_stack_preview();
                BlockWorkspaceEditor.refresh_content_bounds();

                for (const snapshot of snapshots) {
                    // Destroyed clones are already out of the document.
                    if (!snapshot.element.isConnected) continue;

                    snapshot.element.style.zIndex = snapshot.originalZIndex;
                    snapshot.element.style.cursor = "grab";
                }

            };

            const onPointerUp = (upEvent: PointerEvent): void => finish(upEvent, true);
            const onPointerCancel = (cancelEvent: PointerEvent): void => finish(cancelEvent, false);

            window.addEventListener("pointermove", move, true);
            window.addEventListener("pointerup", onPointerUp, true);
            window.addEventListener("pointercancel", onPointerCancel, true);

            return true;
        }

        public apply_drag(): void {
            const element = this.element;
            if (!element) return;

            element.style.touchAction = "none";

            if (this.is_template) {
                element.style.cursor = "copy";
                element.addEventListener("pointerdown", (event: PointerEvent) => {
                    if (event.button !== 0) return;
                    if (event.target instanceof Element && event.target.closest("input, textarea, select, button")) return;
                    event.preventDefault();
                    event.stopPropagation();
                    this.create_template_instance(event);
                });
                return;
            }

            element.style.cursor = "grab";
            element.addEventListener("pointerdown", (event: PointerEvent) => {
                // Reporter SVGs rendered inside a toolbox template are part of the
                // prototype: let the template receive the event instead of moving
                // that embedded prototype reporter out of its socket.
                const templateOwner = element.parentElement?.closest<SVGSVGElement>("[data-block-template='true']");
                if (templateOwner) return;
                this.begin_drag(event);
            });
        }

    }
}

export namespace BlockWorkspaceEditor {
    let workspace_root: HTMLElement | null = null;
    let toolbox_root: HTMLElement | null = null;

    /** Pan/zoom state of the canvas: screen = frame + translate + scale * world. */
    interface WorkspaceViewport {
        scale: number;
        x: number;
        y: number;
    }

    const MIN_ZOOM = 0.25;
    const MAX_ZOOM = 2.5;
    const ZOOM_STEP = 1.2;

    let viewport: WorkspaceViewport = { scale: 1, x: 0, y: 0 };
    const depth_installed = new WeakSet<HTMLElement>();

    /** The stationary window that clips the world. */
    let frame_root: HTMLElement | null = null;
    /** The moving world layer that holds the blocks. */
    let canvas_root: HTMLElement | null = null;

    const grid_style = {
        size: 24,
        minor: "rgba(32, 41, 79, 0.10)",
        major_every: 4,
        major: "rgba(32, 41, 79, 0.22)",
        /** The Blockly-style dot sheet used when `debug` is off. */
        dot: "rgba(32, 41, 79, 0.30)",
    };
    let debug_enabled = true;
    let grid_enabled = true;
    let bounds_enabled = true;

    function grid_visible(): boolean {
        return grid_enabled;
    }

    function bounds_visible(): boolean {
        return debug_enabled && bounds_enabled;
    }
    let fill_height_enabled = true;
    let blur_installed = false;

    /** Register the editor canvas used when toolbox templates spawn instances. */
    export function set_workspace(target: HTMLElement | null): void {
        workspace_root = target;

        if (target) {
            target.dataset.blockWorkspace = "true";
            if (getComputedStyle(target).position === "static") {
                target.style.position = "relative";
            }
        }
    }

    /** Register the toolbox surface. Cloned template instances dragged back here are destroyed. */
    export function set_toolbox(target: HTMLElement | null): void {
        toolbox_root = target;
        if (target) target.dataset.blockToolbox = "true";
    }

    /** True when a pointer is currently over the registered/marked toolbox. */
    export function is_point_in_toolbox(client_x: number, client_y: number): boolean {
        // The panel that currently holds the toolbox templates *is* the toolbox,
        // whether or not set_toolbox() was ever called for it. Without this a
        // custom palette class ("block-palette-toolbox") was never recognised and
        // dropped instances were never deleted.
        const template_hosts = BlockBuilderMachine.CreatedBlocks
            .filter(block => block.is_template)
            .map(block => block.element?.parentElement ?? null);

        const candidates = [
            toolbox_root,
            ...template_hosts,
            ...Array.from(document.querySelectorAll<HTMLElement>(
                '[data-block-toolbox], .block-toolbox, #block-panel, #block-toolbox, .block-palette-toolbox',
            )),
        ].filter((node): node is HTMLElement => !!node?.isConnected);

        return candidates.some(node => {
            const rect = node.getBoundingClientRect();
            return client_x >= rect.left && client_x <= rect.right &&
                client_y >= rect.top && client_y <= rect.bottom;
        });
    }

    /** Resolve a registered workspace, a marked workspace, or the template's parent. */
    export function get_workspace(source?: Element | null): HTMLElement | null {
        // With the viewport on, spawned blocks belong to the world layer, not the
        // frame: they have to pan and zoom with everything else.
        if (canvas_root?.isConnected) return canvas_root;
        if (workspace_root?.isConnected) return workspace_root;

        const local = source?.closest<HTMLElement>(
            "[data-block-workspace], .block-workspace, [data-editor-workspace]",
        ) ?? null;
        const marked = document.querySelector<HTMLElement>(
            "[data-block-workspace], .block-workspace, [data-editor-workspace]",
        );

        return local ?? marked ?? source?.parentElement ?? null;
    }

    // ---------------------------------------------------------------- viewport

    function clamp_zoom(scale: number): number {
        return Math.min(MAX_ZOOM, Math.max(MIN_ZOOM, scale));
    }

    /**
     * The workspace's own layout box, in page pixels, with the viewport transform
     * ignored. Read through a temporary transform reset so the numbers are the
     * frame the user actually looks through, not the scaled content box.
     */
    function get_frame(): HTMLElement | null {
        return frame_root?.isConnected ? frame_root : workspace_root;
    }

    /** Accept a world layer, a frame or a block and hand back the frame element. */
    function resolve_frame(node: HTMLElement | null): HTMLElement | null {
        const frame = get_frame();
        if (!node) return frame;
        if (node.dataset.blockCanvas === "true") return node.parentElement ?? frame;
        if (node.dataset.blockViewport === "true") return node;
        return frame ?? node;
    }

    function get_frame_rect(node: HTMLElement | null): DOMRect {
        const frame = resolve_frame(node);
        if (!frame) return new DOMRect(0, 0, 0, 0);

        // The frame is never transformed, but keep reading it through a reset so a
        // stray transform on it can never skew the window measurements.
        const transform = frame.style.transform;
        frame.style.transform = "none";
        const rect = frame.getBoundingClientRect();
        frame.style.transform = transform;
        return rect;
    }

    /**
     * The world layer: blocks live here and this is what pans and zooms. It is a
     * zero-size origin point, so blocks may sit at any coordinate, negative
     * included, and the layer itself never needs to grow.
     */
    function ensure_canvas(frame: HTMLElement): HTMLElement {
        let canvas = frame.querySelector<HTMLElement>("[data-block-canvas]");

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

            // Blocks already in the frame keep their coordinates: the world layer
            // sits at the frame's padding origin, the reference blocks already use.
            const existing = Array.from(frame.children);
            frame.appendChild(canvas);
            for (const child of existing) {
                if (child === canvas) continue;
                // Panels that belong to the window, not the world, stay put.
                if (child instanceof HTMLElement &&
                    (child.dataset.blockViewportControls || child.dataset.blockContentBoundsLabel)) {
                    continue;
                }
                canvas.appendChild(child);
            }
        }

        return canvas;
    }

    /** The world layer, when the viewport is installed. */
    export function get_canvas(): HTMLElement | null {
        return canvas_root?.isConnected ? canvas_root : null;
    }

    /**
     * Stretch the editor to the bottom of the window.
     *
     * A percentage height only resolves against an ancestor that has one, so a
     * page whose body is auto-height leaves the editor stopping well short of the
     * window bottom. Growing only (until we own the height) keeps a deliberate
     * page size intact.
     */
    export function fit_frame_to_viewport(): void {
        const frame = get_frame();
        if (!frame) return;

        const rect = frame.getBoundingClientRect();
        const available = Math.round(window.innerHeight - rect.top);
        if (available <= 0) return;

        const filled = frame.dataset.blockFilled === "true";
        if (!filled && rect.height >= available - 1) return;

        frame.dataset.blockFilled = "true";
        frame.style.height = `${available}px`;
        sync_bounds_label();
    }

    /**
     * Clicking off a value field has to end the edit.
     *
     * The canvas pan handler calls preventDefault, so the browser never moves focus
     * out of the field itself; and because blocks stop propagation on pointerdown,
     * a per-block handler would miss clicks on other blocks. One capture listener
     * on the document covers the canvas, other blocks, the palette and the page.
     */
    function install_input_blur(): void {
        if (blur_installed) return;
        blur_installed = true;

        document.addEventListener("pointerdown", (event: PointerEvent) => {
            const active = document.activeElement;

            if (!(active instanceof HTMLInputElement) && !(active instanceof HTMLTextAreaElement)) return;
            if (!active.classList.contains("block-input")) return;

            const target = event.target;

            // Clicking the field itself, or another field, keeps editing.
            if (target instanceof Node && (target === active || active.contains(target))) return;
            if (target instanceof Element && target.closest("input, textarea, select")) return;

            active.blur();
        }, true);
    }

    /**
     * Give the canvas a real box to be clipped to.
     *
     * Block children are absolutely positioned, so they contribute no size: a
     * canvas whose own size is `auto` measures 0x0, and clipping that hides every
     * block on the canvas. Returns whether the frame ended up with a usable size.
     */
    function ensure_viewport_frame(workspace: HTMLElement): boolean {
        const parent = workspace.parentElement;

        if (parent && workspace.clientWidth === 0) {
            if (getComputedStyle(parent).display.includes("flex")) {
                workspace.style.flex = "1 1 auto";
                workspace.style.minWidth = "0";
            } else {
                workspace.style.width = "100%";
            }
        }

        if (parent && workspace.clientHeight === 0) {
            workspace.style.height = "100%";
        }

        return workspace.clientWidth > 0 && workspace.clientHeight > 0;
    }

    function apply_viewport_transform(): void {
        const canvas = canvas_root?.isConnected ? canvas_root : null;
        const frame = get_frame();

        // Only the world moves. The frame keeps its box, so the clip edge and the
        // grid stay where the user can actually see them.
        if (canvas) {
            canvas.style.transformOrigin = "0 0";
            canvas.style.transform =
                `translate(${viewport.x}px, ${viewport.y}px) scale(${viewport.scale})`;
        } else if (frame) {
            frame.style.transformOrigin = "0 0";
            frame.style.transform =
                `translate(${viewport.x}px, ${viewport.y}px) scale(${viewport.scale})`;
        }

        sync_viewport_grid();
        sync_bounds_label();
        sync_viewport_readout();
    }

    /**
     * Paint a grid that pans and zooms with the canvas.
     *
     * The grid lives in the canvas's own background, so it is clipped by the
     * canvas box exactly like the blocks are: wherever the grid stops is exactly
     * where content gets clipped, which is what makes the frame readable.
     */
    function sync_viewport_grid(): void {
        const workspace = get_frame();
        if (!workspace) return;

        // The frame outline is a debug aid, so it follows `debug`, not `grid`.
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

        // Production look: Blockly's dot sheet. Debug swaps it for the full
        // crosshatch, which is what makes block boxes and gutters easy to measure.
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

    /** Turn the canvas grid on or off, optionally restyling it. */
    export function set_grid(
        visible: boolean,
        options: Partial<typeof grid_style> = {},
    ): void {
        grid_enabled = visible;
        Object.assign(grid_style, options);
        sync_viewport_grid();
    }

    /**
     * Turn every debug aid on or off at once: the frame outline, the content box and
     * its size label, and the grid style (full crosshatch on, Blockly dot sheet
     * off). The dot sheet always stays, so the canvas never goes blank; use
     * `set_grid(false)` for that. Individual switches keep their setting underneath,
     * so `set_debug(false)` then `set_debug(true)` restores what you had.
     */
    export function set_debug(visible: boolean): void {
        debug_enabled = visible;
        sync_viewport_grid();
        refresh_content_bounds();
    }

    /** Show or hide just the dashed content box and its size label. */
    export function set_content_bounds(visible: boolean): void {
        bounds_enabled = visible;
        refresh_content_bounds();
    }

    /** Current debug state, for wiring a keyboard shortcut or a menu item. */
    export function get_debug(): boolean {
        return debug_enabled;
    }

    interface BoundsBox {
        x: number;
        y: number;
        width: number;
        height: number;
    }

    /** Last measured content bbox, in world units, for the screen-space label. */
    let bounds_world: BoundsBox | null = null;

    function ensure_bounds_label(frame: HTMLElement): HTMLElement {
        let label = frame.querySelector<HTMLElement>("[data-block-content-bounds-label]");

        if (!label) {
            label = document.createElement("div");
            label.dataset.blockContentBoundsLabel = "true";
            Object.assign(label.style, {
                position: "absolute",
                left: "0",
                top: "0",
                // Above every block: the label must never be covered by the very
                // content it is measuring.
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

    /**
     * Place the size readout in screen space, just outside the bounds box.
     *
     * It lives in the frame rather than the world layer for two reasons: it stays
     * legible at any zoom, and it can sit above the content instead of under it.
     */
    function sync_bounds_label(): void {
        const frame = get_frame();
        if (!frame) return;

        const label = frame.querySelector<HTMLElement>("[data-block-content-bounds-label]");
        if (!label) return;

        if (!bounds_world) {
            label.style.display = "none";
            return;
        }

        const scale = viewport.scale || 1;
        const { x, y, width, height } = bounds_world;

        const left = viewport.x + x * scale;
        const top = viewport.y + y * scale;
        const bottom = viewport.y + (y + height) * scale;

        // jsdom reports 0 for both; the fallback keeps the maths sane there.
        const label_width = label.offsetWidth || 84;
        const label_height = label.offsetHeight || 20;

        const above = top - label_height - 2;
        const below = bottom + 2;

        let label_top: number;
        if (above >= 0) {
            label_top = above;
        } else if (below + label_height <= frame.clientHeight) {
            label_top = below;
        } else {
            label_top = Math.max(0, Math.min(top + 2, frame.clientHeight - label_height));
        }

        label.style.display = "block";
        label.style.left = `${Math.max(0, Math.min(left, frame.clientWidth - label_width))}px`;
        label.style.top = `${label_top}px`;
    }

    /** Dashed box around everything on the canvas, so content vs frame is obvious. */
    export function refresh_content_bounds(): void {
        const workspace = get_frame();
        const world = canvas_root?.isConnected ? canvas_root : workspace;
        if (!workspace || !world) return;

        if (!bounds_visible()) {
            const hidden = world.querySelector<HTMLElement>("[data-block-content-bounds]");
            if (hidden) hidden.style.display = "none";
            bounds_world = null;
            sync_bounds_label();
            return;
        }

        let node = world.querySelector<HTMLElement>("[data-block-content-bounds]");
        if (!node) {
            node = document.createElement("div");
            node.dataset.blockContentBounds = "true";
            Object.assign(node.style, {
                position: "absolute",
                pointerEvents: "none",
                // Over the blocks, so the outline is visible where content meets it.
                zIndex: "2147482000",
                border: "1px dashed #d14343",
                borderRadius: "4px",
                boxSizing: "border-box",
                display: "none",
            });
            world.appendChild(node);
        }

        const blocks = BlockBuilderMachine.CreatedBlocks.filter(
            block => block.element?.isConnected && !block.is_template,
        );

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
            if (!element) continue;

            const rect = element.getBoundingClientRect();
            const left = (rect.left - frame.left - viewport.x) / scale;
            const top = (rect.top - frame.top - viewport.y) / scale;

            min_x = Math.min(min_x, left);
            min_y = Math.min(min_y, top);
            max_x = Math.max(max_x, left + rect.width / scale);
            max_y = Math.max(max_y, top + rect.height / scale);
        }

        if (!Number.isFinite(min_x) || !Number.isFinite(min_y)) return;

        bounds_world = {
            x: min_x,
            y: min_y,
            width: max_x - min_x,
            height: max_y - min_y,
        };

        // This node sits in the world layer, so plain canvas coordinates hold;
        // panning and zooming move it with the blocks for free.
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

    function sync_viewport_readout(): void {
        const readout = document.querySelector<HTMLElement>("[data-block-zoom-readout]");
        if (readout) readout.textContent = `${Math.round(viewport.scale * 100)}%`;
    }

    /** Current canvas zoom, as a multiplier (1 = 100%). */
    export function get_zoom(): number {
        return viewport.scale;
    }

    /** Canvas zoom as a divisor for screen-pixel measurements. Never zero. */
    export function get_workspace_scale(): number {
        return viewport.scale || 1;
    }

    /**
     * Screen pixels → workspace units. Block geometry is stored unscaled, so every
     * pointer delta has to go through this or a drag would move twice as far at 2x.
     */
    export function to_workspace_delta(value: number): number {
        return value / (viewport.scale || 1);
    }

    /** Place an element at a viewport (client) position, in canvas coordinates. */
    export function set_workspace_position(
        element: HTMLElement | SVGElement,
        client_left: number,
        client_top: number,
        workspace: HTMLElement | null = workspace_root,
    ): void {
        if (!workspace) return;

        if (getComputedStyle(workspace).position === "static") {
            workspace.style.position = "relative";
        }

        const frame = get_frame_rect(workspace);
        const scale = viewport.scale || 1;

        element.style.left = `${
            (client_left - frame.left - viewport.x) / scale - workspace.clientLeft + workspace.scrollLeft
        }px`;
        element.style.top = `${
            (client_top - frame.top - viewport.y) / scale - workspace.clientTop + workspace.scrollTop
        }px`;
    }

    /** Zoom to an absolute scale, keeping the point under (x, y) fixed. */
    export function set_zoom(scale: number, client_x?: number, client_y?: number): void {
        const workspace = workspace_root;
        if (!workspace) return;

        const next = clamp_zoom(scale);
        if (Math.abs(next - viewport.scale) < 0.001) return;

        const frame = get_frame_rect(workspace);
        // Without a sized frame the "middle" would be the canvas origin, which
        // throws the content off screen; anchor on that origin instead.
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

    export function zoom_in(factor: number = ZOOM_STEP): void {
        set_zoom(viewport.scale * factor);
    }

    export function zoom_out(factor: number = ZOOM_STEP): void {
        set_zoom(viewport.scale / factor);
    }

    /** Move the canvas by a screen-pixel delta. */
    export function pan_by(delta_x: number, delta_y: number): void {
        viewport.x += delta_x;
        viewport.y += delta_y;
        apply_viewport_transform();
    }

    /** Put every real block in the middle of the viewport, at the current zoom. */
    export function center_viewport(padding: number = 32): void {
        const workspace = workspace_root;
        if (!workspace) return;

        const frame = get_frame_rect(workspace);
        const scale = viewport.scale || 1;
        const blocks = BlockBuilderMachine.CreatedBlocks.filter(
            block => block.element?.isConnected && !block.is_template,
        );

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
            if (!element) continue;

            const rect = element.getBoundingClientRect();
            const left = (rect.left - frame.left - viewport.x) / scale;
            const top = (rect.top - frame.top - viewport.y) / scale;

            min_x = Math.min(min_x, left);
            min_y = Math.min(min_y, top);
            max_x = Math.max(max_x, left + rect.width / scale);
            max_y = Math.max(max_y, top + rect.height / scale);
        }

        if (!Number.isFinite(min_x) || !Number.isFinite(min_y)) return;

        const usable_width = Math.max(0, frame.width - padding * 2);
        const usable_height = Math.max(0, frame.height - padding * 2);

        viewport.x = padding + (usable_width - (max_x - min_x) * scale) / 2 - min_x * scale;
        viewport.y = padding + (usable_height - (max_y - min_y) * scale) / 2 - min_y * scale;

        apply_viewport_transform();
        refresh_content_bounds();
    }

    /** Build the zoom in / zoom out / center controls once, into the frame's host. */
    /**
     * Build the zoom controls inside the viewport itself, bottom left.
     *
     * They live in the frame rather than the frame's parent: the frame is the one
     * box that is guaranteed to be visible and to stay put, so the buttons cannot
     * land off screen or behind another panel. Pass a host to mount them somewhere
     * else instead.
     */
    export function mount_viewport_controls(host: HTMLElement | null = null): void {
        const target = host ?? get_frame() ?? workspace_root?.parentElement ?? null;
        if (!target) return;

        if (getComputedStyle(target).position === "static") {
            target.style.position = "relative";
        }

        // One bar per editor, re-homed rather than duplicated when the mount point
        // changes or the depth install runs again.
        let bar = document.querySelector<HTMLElement>("[data-block-viewport-controls]");
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

            const make_button = (label: string, title: string, action: () => void, wide = false): void => {
                const button = document.createElement("button");
                button.type = "button";
                button.textContent = label;
                button.title = title;
                button.setAttribute("aria-label", title);
                Object.assign(button.style, {
                    // Glyphs get a square icon button; a word gets room to breathe.
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
                button.addEventListener("click", (event: MouseEvent) => {
                    event.stopPropagation();
                    action();
                });
                bar!.appendChild(button);
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

    /**
     * Turn the workspace into a pannable, zoomable viewport.
     *
     * Drag empty canvas to pan, ctrl/cmd + wheel to zoom at the cursor, plain wheel
     * to pan, and the mounted controls for zoom in / out / center. Blocks keep
     * unscaled coordinates; only the canvas carries the transform.
     */
    export interface EditorDepthOptions {
        /** Clip the canvas to its own box. Defaults to true when the box has a size. */
        clip?: boolean;
        /**
         * Master switch for the debug aids: the frame outline and the content box
         * with its size label, plus the grid style (`true` draws the full
         * crosshatch, `false` the plain Blockly dot sheet). Defaults to on.
         */
        debug?: boolean;
        /** Draw any background at all. `false` leaves the canvas empty. Defaults to on. */
        grid?: boolean;
        /** Dashed box + size label around all content. Defaults to on. */
        content_bounds?: boolean;
        /** Alias for `content_bounds`. */
        box_size_outline?: boolean;
        /** Stretch the editor to the bottom of the window. Defaults to on. */
        fill_height?: boolean;
    }

    export function apply_editor_depth(
        target: HTMLElement | null = workspace_root,
        options: EditorDepthOptions = {},
    ): void {
        const workspace = target ?? workspace_root;
        if (!workspace) return;

        frame_root = workspace;

        if (getComputedStyle(workspace).position === "static") {
            workspace.style.position = "relative";
        }

        workspace.dataset.blockViewport = "true";
        workspace.style.touchAction = "none";

        // The world layer is what moves; the frame stays put and clips it.
        canvas_root = ensure_canvas(workspace);

        // Clipping an unsized canvas would hide every block on it, so the frame is
        // measured (and given a size) first, and only a real box gets clipped.
        const framed = ensure_viewport_frame(workspace);
        const clip = options.clip ?? true;

        debug_enabled = options.debug ?? true;
        grid_enabled = options.grid ?? true;
        bounds_enabled = options.content_bounds ?? options.box_size_outline ?? true;
        fill_height_enabled = options.fill_height ?? true;

        if (fill_height_enabled) fit_frame_to_viewport();

        install_input_blur();

        workspace.style.overflow = clip && framed ? "hidden" : "visible";

        if (!framed) {
            console.debug(
                "[block-editor] viewport frame has no size yet, so it is not clipped. " +
                "Give the canvas a width/height (or a sized flex parent) to clip it.",
            );
        }

        apply_viewport_transform();
        refresh_content_bounds();

        if (!depth_installed.has(workspace)) {
            depth_installed.add(workspace);

            let panning: {
                pointer_id: number | undefined;
                x: number;
                y: number;
                origin_x: number;
                origin_y: number;
            } | null = null;

            workspace.addEventListener("pointerdown", (event: PointerEvent) => {
                if (event.button !== 0 && event.button !== 1) return;

                // Blocks stopPropagation on their own pointerdown, so anything that
                // arrives here is empty canvas. Editable controls still have to be
                // ignored: their events bubble out of a block without being stopped.
                if (event.target instanceof Element &&
                    event.target.closest("input, textarea, select, button, [data-block-viewport-controls]")) {
                    return;
                }

                panning = {
                    pointer_id: (event as PointerEvent).pointerId,
                    x: event.clientX,
                    y: event.clientY,
                    origin_x: viewport.x,
                    origin_y: viewport.y,
                };
                workspace.style.cursor = "grabbing";
                event.preventDefault();
            });

            window.addEventListener("pointermove", (event: PointerEvent) => {
                if (!panning) return;
                if (panning.pointer_id !== undefined && event.pointerId !== undefined &&
                    event.pointerId !== panning.pointer_id) return;

                viewport.x = panning.origin_x + (event.clientX - panning.x);
                viewport.y = panning.origin_y + (event.clientY - panning.y);
                apply_viewport_transform();
            }, true);

            const end_pan = (): void => {
                if (!panning) return;
                panning = null;
                workspace.style.cursor = "";
            };

            window.addEventListener("pointerup", end_pan, true);
            window.addEventListener("pointercancel", end_pan, true);

            window.addEventListener("resize", () => {
                if (fill_height_enabled) fit_frame_to_viewport();
                apply_viewport_transform();
            });

            workspace.addEventListener("wheel", (event: WheelEvent) => {
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
}
