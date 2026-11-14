export var BlockBuilderMachine;
(function (BlockBuilderMachine) {
    let OutputBlocks;
    (function (OutputBlocks) {
        OutputBlocks[OutputBlocks["STRING"] = 0] = "STRING";
        OutputBlocks[OutputBlocks["BOOL"] = 1] = "BOOL";
    })(OutputBlocks = BlockBuilderMachine.OutputBlocks || (BlockBuilderMachine.OutputBlocks = {}));
    class Block {
        id;
        constructor() {
            this.id = 0;
        }
    }
    BlockBuilderMachine.Block = Block;
    class OutputBlock extends Block {
        params;
        type;
        element;
        constructor(params, type) {
            super();
            this.params = params;
            this.type = type;
            this.element = null;
        }
        invoke() {
        }
    }
    BlockBuilderMachine.OutputBlock = OutputBlock;
})(BlockBuilderMachine || (BlockBuilderMachine = {}));
