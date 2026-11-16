import { BlockBuilderMachine } from "./block_constructor.js";

export namespace VirtualMachine {
    export interface BlockDefinition {
        id: string;
        execute?: (inputs: Map<number, BlockBuilderMachine.BlockInput>, context: unknown) => unknown;
        proto: BlockBuilderMachine.Block;
        type: BlockBuilderMachine.OutputBlockTypes;
    }

    export class Machine {
        private block_registry: BlockDefinition[] = [];

        public inject_block(proto: BlockBuilderMachine.Block): void {
            const existing = this.block_registry.find(block => block.id === proto.id);
            if (existing) { throw new Error(`Block '${proto.id}' is already registered.`); }
            const index = this.block_registry.push({ id: proto.id, type: proto.type, proto });
            console.debug("PARSED BLOCK INTO VM:\t", proto);
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
    // TODO...
}
