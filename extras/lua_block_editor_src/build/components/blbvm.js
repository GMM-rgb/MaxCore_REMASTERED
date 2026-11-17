export var VirtualMachine;
(function (VirtualMachine) {
    class Machine {
        options;
        block_registry = [];
        constructor(options) {
            this.options = options;
        }
        get function_op_template() {
            return { template: Function("inputs", "context") };
        }
        inject_block(proto) {
            const existing = this.block_registry.find(block => block.id === proto.id);
            if (existing) {
                throw new Error(`Block '${proto.id}' is already registered.`);
            }
            let config = { id: proto.id, type: proto.type, proto };
            this.block_registry.push(config);
            console.debug("PARSED BLOCK INTO VM:\t", this.get_block(proto.id));
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
export var VirtualMachineParser;
(function (VirtualMachineParser) {
    function parse_generation(target) {
        let SearchGenerator = new RegExp(/(<=?[]+)\1/);
        let ParamContents = [];
        target.proto.inputs.forEach((i) => {
            const InputContent = new String(i?.content ?? null).valueOf();
            console.debug("PARSING INPUT:\t", InputContent);
            ParamContents.push(InputContent);
        });
        return ``;
    }
    VirtualMachineParser.parse_generation = parse_generation;
})(VirtualMachineParser || (VirtualMachineParser = {}));
