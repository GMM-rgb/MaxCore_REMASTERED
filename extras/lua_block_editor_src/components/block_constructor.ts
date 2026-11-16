
export namespace BlockBuilderMachine {
    export enum CommandBlocks {
        COM = 0,
        HAT = 1,
        END = 2,
    }

    export enum OutputBlocks {
        STRING = 0,
        BOOL = 1,
    }

    export type OutputBlockTypes = CommandBlocks | OutputBlocks;

    export interface BlockInput {
        width: number;
    }

    export interface BlockSize {
        width: number;
        height: number;
    }

    export interface BlockGeometry {
        radius: number;
        notch: { width: number; depth: number };
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
}

export class Block {
    public element: SVGSVGElement | null;
    public inputs: BlockInput[];
    public label: string;

    private measurement_svg: SVGSVGElement | null = null;
    private measurement_text: SVGTextElement | null = null;

    constructor(private params: BlockConfig, private type: OutputBlockTypes, private text: string) {
        this.element = null;
        this.label = text.trim();
        this.inputs = [];
    }

    private get_measurement_elements(): SVGTextElement {
        if (!this.measurement_svg || !this.measurement_text) {
            const namespace = "http://www.w3.org/2000/svg";
            const svg = document.createElementNS(namespace, "svg");
            const text = document.createElementNS(namespace, "text");

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

    private append_formatted_text(target: SVGTextElement | SVGTSpanElement, source: string): void {
        const parsed = new DOMParser().parseFromString(source, "text/html");

        const append_nodes = (parent: Node, destination: SVGTextElement | SVGTSpanElement): void => {
            for (const node of Array.from(parent.childNodes)) {
                if (node.nodeType === Node.TEXT_NODE) {
                    destination.appendChild(document.createTextNode(node.textContent ?? ""));
                    continue;
                }

                if (node.nodeType !== Node.ELEMENT_NODE) continue;

                const element = node as Element;
                const tag = element.tagName.toLowerCase();
                const tspan = document.createElementNS("http://www.w3.org/2000/svg", "tspan");

                switch (tag) {
                    case "b":
                        // continue to next
                    case "strong":
                        tspan.style.fontWeight = "bold";
                        break;
                    case "i":
                        // continue to next
                    case "em":
                        tspan.style.fontStyle = "italic";
                        break;
                    case "u":
                        tspan.style.textDecoration = "underline";
                        break;
                    case "s":
                        // continue to next
                    case "del":
                        tspan.style.textDecoration = "line-through";
                        break;
                    case "code":
                        tspan.style.fontFamily = "monospace";
                        break;
                }

                append_nodes(element, tspan);
                destination.appendChild(tspan);
            }
        };

        append_nodes(parsed.body, target);
    }

    protected measure_text(text: string, font: string = this.params.font): number {
        const svg_text = this.get_measurement_elements();

        svg_text.replaceChildren();
        svg_text.style.font = font;

        this.append_formatted_text(svg_text, text);

        return svg_text.getComputedTextLength();
    }

    private calculate_stroke_color(fill: string): string {
        const color = fill.match(/^#([0-9a-f]{6})$/i);

        if (!color) return fill;

        const hex = color[1];
        const channels = [0, 2, 4].map(index => parseInt(hex.substring(index, index + 2), 16));

        const darker = channels.map(channel => Math.round(channel * 0.7));

        return `#${darker.map(channel => channel.toString(16).padStart(2, "0")).join("")}`;
    }

    public calculate_block_size(): BlockSize {
        const paddingX = 12;
        const inputWidth = this.inputs.reduce((total, input) => total + input.width + 8, 0);
        const contentWidth = this.measure_text(this.label) + inputWidth;

        return {
            width: contentWidth + paddingX * 2,
            height: 48
        };
    }

    private construct_block_contents(config: BlockConfig, size: BlockSize): string {
        const geometry = config.geometry ?? {
            radius: 5,
            notch: { width: 14, depth: 8 },
            top_notch: false,
            bottom_connector: false
        };

        const { radius, notch } = geometry;
        const { width, height } = size;
        const { width: notchWidth, depth: notchDepth } = notch;
        const center = width / 2;
        const commands: string[] = [];

        commands.push(`M 0 ${radius}`);
        commands.push(`L 0 ${height - radius}`);
        commands.push(`Q 0 ${height} ${radius} ${height}`);

        if (geometry.bottom_connector) {
            commands.push(`L ${center - notchWidth} ${height}`);
            commands.push(`L ${center - notchWidth} ${height + notchDepth}`);
            commands.push(`L ${center + notchWidth} ${height + notchDepth}`);
            commands.push(`L ${center + notchWidth} ${height}`);
        }

        commands.push(`L ${width - radius} ${height}`);
        commands.push(`Q ${width} ${height} ${width} ${height - radius}`);
        commands.push(`L ${width} ${radius}`);
        commands.push(`Q ${width} 0 ${width - radius} 0`);

        if (geometry.top_notch) {
            commands.push(`L ${center + notchWidth} 0`);
            commands.push(`L ${center + notchWidth} ${notchDepth}`);
            commands.push(`L ${center - notchWidth} ${notchDepth}`);
            commands.push(`L ${center - notchWidth} 0`);
        }

        commands.push(`L ${radius} 0`);
        commands.push(`Q 0 0 0 ${radius}`);
        commands.push("Z");

        return commands.join("\n");
    }

    public invoke(): SVGSVGElement {
        const namespace = "http://www.w3.org/2000/svg";
        const size = this.calculate_block_size();

        const strokeWidth = this.params.stroke_width ?? 1.5;
        const strokeOffset = strokeWidth / 2;
        const extraHeight = this.params.geometry?.bottom_connector ? this.params.geometry.notch.depth : 0;
        const totalWidth = size.width + strokeWidth;
        const totalHeight = size.height + extraHeight + strokeWidth;

        const svg = document.createElementNS(namespace, "svg");
        const path = document.createElementNS(namespace, "path");
        const text = document.createElementNS(namespace, "text");

        path.setAttribute("d", this.construct_block_contents(this.params, size));
        path.setAttribute("fill", this.params.fill);
        path.setAttribute("stroke", this.params.stroke ?? this.calculate_stroke_color(this.params.fill));        path.setAttribute("stroke-width", String(strokeWidth));
        path.setAttribute("stroke-linejoin", "round");
        path.setAttribute("transform", `translate(${strokeOffset} ${strokeOffset})`);

        text.style.font = this.params.font;
        text.setAttribute("fill", this.params.text_fill ?? "#111111");
        text.setAttribute("x", String(12 + strokeOffset));
        text.setAttribute("y", String(size.height / 2 + strokeOffset));
        text.setAttribute("dominant-baseline", "middle");

        this.append_formatted_text(text, this.label);

        svg.setAttribute("viewBox", `0 0 ${totalWidth} ${totalHeight}`);
        svg.setAttribute("width", String(totalWidth));
        svg.setAttribute("height", String(totalHeight));

        svg.appendChild(path);
        svg.appendChild(text);

        this.element?.remove();
        this.element = svg;

        return svg;
    }

    public destroy(): void {
        this.element?.remove();
        this.measurement_svg?.remove();

        this.measurement_svg = null;
        this.measurement_text = null;
        this.element = null;
    }
}
}
