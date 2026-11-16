export var VirtualMachine;
(function (VirtualMachine) {
    class Machine {
        block_registry = [];
        inject_block(proto) {
            const existing = this.block_registry.find(block => block.id === proto.id);
            if (existing) {
                throw new Error(`Block '${proto.id}' is already registered.`);
            }
            const index = this.block_registry.push({ id: proto.id, type: proto.type, proto });
            console.debug("PARSED BLOCK INTO VM:\t", proto);
        }
        get_block(id) {
            return this.block_registry.find(block => block.id === id);
        }
        get_blocks() {
            return this.block_registry;
        }
    }
    VirtualMachine.Machine = Machine;
})(VirtualMachine || (VirtualMachine = {}));
