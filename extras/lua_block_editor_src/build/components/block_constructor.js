var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (Object.prototype.hasOwnProperty.call(b, p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };
    return function (d, b) {
        if (typeof b !== "function" && b !== null)
            throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
export var BlockBuilderMachine;
(function (BlockBuilderMachine) {
    var OutputBlocks;
    (function (OutputBlocks) {
        OutputBlocks[OutputBlocks["STRING"] = 0] = "STRING";
        OutputBlocks[OutputBlocks["BOOL"] = 1] = "BOOL";
    })(OutputBlocks = BlockBuilderMachine.OutputBlocks || (BlockBuilderMachine.OutputBlocks = {}));
    var Block = (function () {
        function Block() {
            this.id = 0;
        }
        return Block;
    }());
    BlockBuilderMachine.Block = Block;
    var OutputBlock = (function (_super) {
        __extends(OutputBlock, _super);
        function OutputBlock(params, type) {
            var _this = _super.call(this) || this;
            _this.params = params;
            _this.type = type;
            _this.element = null;
            return _this;
        }
        OutputBlock.prototype.invoke = function () {
        };
        return OutputBlock;
    }(Block));
    BlockBuilderMachine.OutputBlock = OutputBlock;
})(BlockBuilderMachine || (BlockBuilderMachine = {}));
