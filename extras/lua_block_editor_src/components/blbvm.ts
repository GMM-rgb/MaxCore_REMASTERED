import { BlockBuilderMachine } from "./block_constructor.js";

/**
     $id: Block Lua Builder - Virtual Machine $
*/

type Block = BlockBuilderMachine.Block;
type BlockInput = BlockBuilderMachine.BlockInput;

export namespace VirtualMachine {
    export interface BlockDefinition {
        readonly id: string;
        readonly type: BlockBuilderMachine.OutputBlockTypes;
        readonly proto: Block;
    }

    export class Machine {
        private readonly block_registry = new Map<string, BlockDefinition>();

        public constructor(_options?: unknown) {}

        public inject_block(proto: Block): BlockDefinition {
            const existing = this.block_registry.get(proto.id);

            if (existing) {
                if (existing.proto !== proto) {
                    throw new Error(`Block '${proto.id}' is already registered.`);
                }

                return existing;
            }

            const config: BlockDefinition = {
                id: proto.id,
                type: proto.type,
                proto
            };

            this.block_registry.set(proto.id, config);
            console.debug("REGISTERED BLOCK:", proto.id, proto);
            return config;
        }

        public get_block(id: string): BlockDefinition | undefined {
            return this.block_registry.get(id);
        }

        public remove_block(id: string): void {
            this.block_registry.delete(id);
        }

        public get_blocks(): readonly BlockDefinition[] {
            return [...this.block_registry.values()];
        }
    }
}

export namespace VirtualMachineParser {
    export class LuaCompileError extends Error {
        public constructor(
            message: string,
            public readonly block_id?: string,
            public readonly input_id?: number
        ) {
            super(message);
            this.name = "LuaCompileError";
        }
    }

    function is_reporter(block: Block): boolean {
        return (
            block.type === BlockBuilderMachine.CommandBlocks.REP ||
            block.type === BlockBuilderMachine.OutputBlocks.STRING ||
            block.type === BlockBuilderMachine.OutputBlocks.BOOL
        );
    }

    function quote_lua_string(value: string): string {
        const escaped = value
            .replace(/\\/g, "\\\\")
            .replace(/"/g, '\\"')
            .replace(/\r/g, "\\r")
            .replace(/\n/g, "\\n")
            .replace(/\t/g, "\\t")
            .replace(/[\u0000-\u001F\u007F]/g, character =>
                `\\${character.charCodeAt(0).toString().padStart(3, "0")}`
            );

        return `"${escaped}"`;
    }

    function compile_literal(
        input: BlockInput,
        block: Block,
        input_id: number
    ): string {
        switch (input.type) {
            case BlockBuilderMachine.InputTypes.STRING:
                return quote_lua_string(String(input.content));

            case BlockBuilderMachine.InputTypes.NUMBER:
                if (
                    typeof input.content !== "number" ||
                    !Number.isFinite(input.content)
                ) {
                    throw new LuaCompileError(
                        `Invalid number in input ${input_id}.`,
                        block.id,
                        input_id
                    );
                }

                return Object.is(input.content, -0) ? "0" : String(input.content);

            case BlockBuilderMachine.InputTypes.BOOL:
                if (typeof input.content !== "boolean") {
                    throw new LuaCompileError(
                        `Invalid boolean in input ${input_id}.`,
                        block.id,
                        input_id
                    );
                }

                return input.content ? "true" : "false";

            case BlockBuilderMachine.InputTypes.VARIABLE: {
                const variable = String(input.content).trim();

                if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(variable)) {
                    throw new LuaCompileError(
                        `Invalid Lua variable name '${variable}'.`,
                        block.id,
                        input_id
                    );
                }

                return variable;
            }

            case BlockBuilderMachine.InputTypes.BLOCK:
                throw new LuaCompileError(
                    `Block '${block.label}' has an empty BLOCK input (${input_id}).`,
                    block.id,
                    input_id
                );

            default:
                throw new LuaCompileError(
                    `Unknown input type in block '${block.label}'.`,
                    block.id,
                    input_id
                );
        }
    }

    function compile_input(
        block: Block,
        input_id: number,
        ancestry: Set<Block>
    ): string {
        // Template placeholders address logical input IDs. The connection map
        // is keyed by the input's array position, so resolve ID -> index first.
        const input_index = block.inputs.findIndex(
            (input, index) => (input.input_id ?? index) === input_id
        );

        if (input_index === -1) {
            throw new LuaCompileError(
                `Template references nonexistent input {${input_id}} in '${block.label}'.`,
                block.id,
                input_id
            );
        }

        const input = block.inputs[input_index];
        const connected = block.get_connected_input(input_index);

        if (connected) {
            if (!is_reporter(connected)) {
                throw new LuaCompileError(
                    `Input ${input_id} contains a non-reporter block.`,
                    block.id,
                    input_id
                );
            }

            return compile_expression(connected, ancestry);
        }

        return compile_literal(input, block, input_id);
    }

    function compile_template(block: Block, ancestry: Set<Block>): string {
        if (ancestry.has(block)) {
            throw new LuaCompileError(
                `Circular reporter dependency detected in '${block.label}'.`,
                block.id
            );
        }

        ancestry.add(block);

        try {
            const template = block.lua_op.trim();

            if (!template) {
                if (is_reporter(block)) {
                    throw new LuaCompileError(
                        `Reporter '${block.label}' has no Lua template.`,
                        block.id
                    );
                }

                return "";
            }

            return template.replace(
                /\{(\d+)\}/g,
                (_match: string, raw_id: string): string => {
                    const input_id = Number(raw_id);

                    if (!Number.isSafeInteger(input_id)) {
                        throw new LuaCompileError(
                            `Invalid template parameter: {${raw_id}}.`,
                            block.id
                        );
                    }

                    const value = compile_input(block, input_id, ancestry);

                    console.debug(
                        "RESOLVED PARAMETER:",
                        block.label,
                        input_id,
                        value
                    );

                    return value;
                }
            );
        } finally {
            ancestry.delete(block);
        }
    }

    export function compile_expression(
        block: Block,
        ancestry: Set<Block> = new Set()
    ): string {
        if (!is_reporter(block)) {
            throw new LuaCompileError(
                `Block '${block.label}' is not an expression block.`,
                block.id
            );
        }

        return compile_template(block, ancestry);
    }

    /**
     * Compile the complete stack containing `start`.
     *
     * `start` does not have to be the stack root. This matters after a drop:
     * when a dragged block is snapped UNDER an existing stack, its parent
     * pointer now leads to the original stack's root.
     */
    export function compile_stack(start: Block): string {
        // Normalize in the parent direction first.
        const parent_path = new Set<Block>();
        let root = start;

        while (true) {
            if (parent_path.has(root)) {
                throw new LuaCompileError(
                    "Circular parent connection detected while finding stack root.",
                    root.id
                );
            }

            parent_path.add(root);

            const parent = root.get_parent_block();
            if (!parent) break;

            root = parent;
        }

        // Then compile the stack in execution order (top -> bottom).
        const visited = new Set<Block>();
        const statements: string[] = [];
        let current: Block | null = root;

        while (current) {
            if (visited.has(current)) {
                throw new LuaCompileError(
                    "Circular command-stack connection detected.",
                    current.id
                );
            }

            visited.add(current);

            if (is_reporter(current)) {
                throw new LuaCompileError(
                    `Reporter '${current.label}' cannot be compiled as a command.`,
                    current.id
                );
            }

            const generated = compile_template(current, new Set());

            if (generated.trim()) {
                statements.push(generated);
            }

            current = current.get_next_block();
        }

        const output = statements.join("\n");

        console.debug(
            "COMPILED STACK:",
            root.id,
            [...visited].map(block => block.label),
            output
        );

        return output;
    }

    export function compile_workspace(
        blocks: readonly Block[] = BlockBuilderMachine.CreatedBlocks
    ): string {
        const roots = blocks.filter(block =>
            !block.is_template &&
            !is_reporter(block) &&
            block.get_parent_block() === null
        );

        const output: string[] = [];

        for (const root of roots) {
            const generated = compile_stack(root);
            if (generated.trim()) output.push(generated);
        }

        return output.join("\n\n");
    }

    export function parse_generation(
        target: VirtualMachine.BlockDefinition
    ): string {
        const block = target.proto;

        const generated = is_reporter(block)
            ? compile_expression(block)
            : compile_stack(block);

        console.debug("GENERATED LUA:", generated);
        return generated;
    }
}
