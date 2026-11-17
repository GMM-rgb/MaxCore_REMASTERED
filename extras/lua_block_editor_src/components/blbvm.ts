import { BlockBuilderMachine } from "./block_constructor.js";

export namespace VirtualMachine {
    export interface BlockDefinition {
        execute?: <ReturnType>(inputs: BlockBuilderMachine.BlockInput[], context: Object) => ReturnType;
        type: BlockBuilderMachine.OutputBlockTypes; proto: BlockBuilderMachine.Block; id: string;
    }

    export class Machine {
        private block_registry: BlockDefinition[] = [];

        public constructor(private options?: any) {}

        public get function_op_template(): { template: BlockDefinition["execute"] } {
            return { template: (Function("inputs", "context") as BlockDefinition["execute"]) };
        }

        public inject_block(proto: BlockBuilderMachine.Block): void {
            const existing = this.block_registry.find(block => block.id === proto.id);
            if (existing) { throw new Error(`Block '${proto.id}' is already registered.`); }
            let config: BlockDefinition = { id: proto.id, type: proto.type, proto };
            this.block_registry.push(config);
            console.debug("PARSED BLOCK INTO VM:\t", this.get_block(proto.id));
        }

        public get_block(id: string): BlockDefinition | undefined {
            return this.block_registry.find(block => block.id === id);
        }

        public get_blocks(): readonly BlockDefinition[] {
            return this.block_registry;
        }
    }
}

export namespace VirtualMachineParser {
    export function parse_generation(target: VirtualMachine.BlockDefinition): string {
        let SearchGenerator = new RegExp(/(<=?[]+)\1/);
        let ParamContents: string[] = [];

        target.proto.inputs.forEach((i: BlockBuilderMachine.BlockInput): void => {
            const InputContent = new String(i?.content ?? null).valueOf();
            console.debug("PARSING INPUT:\t", InputContent);
            ParamContents.push(InputContent);
        });

        // target.proto.lua_op.replace("", );

        return ``;
    }
}
