export namespace BlockBuilderMachine {
    type OutputBlockTypes = {};

    export enum OutputBlocks {
        STRING = 0, BOOL = 1,
    }

    export class Block {
        id: number;

        constructor() {
            this.id = 0;
        }
    }

    export class OutputBlock extends Block {
        element: SVGElement | null;
        
        constructor(
            private params: object,
            private type: OutputBlockTypes
        ) {
            super();
            this.element = null;
        }

        public invoke() {
            
        }
    }
}
