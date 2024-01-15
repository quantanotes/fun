const std = @import("std");

const Word = u64;

const Datum = packed union {
    one: Word,
    many: [*]Word,
};

const Opcode = enum(u6) {
    Halt,
    Noop,

    Half,
    Const,
    Local,
    Global,

    Define,
    Set,

    Apply,
    Tail,
    Return,

    JumpEq,
    JumpNeq,
    JumpTrue,
    JumpFalse,

    Add,

    Debug,
};

const Instruction = struct {
    const Args = union {
        a3: struct { u19, u19, u19 },
        a2: struct { u28, u28 },
        ah: struct { u32, u24 },
        a: u56,
        n: struct {},
    };

    op: Opcode,
    args: Args,

    pub inline fn init(op: Opcode, args: Args) Instruction {
        return Instruction{ .op = op, .args = args };
    }

    pub inline fn a3(op: Opcode, arg0: u19, arg1: u19, arg2: u19) Instruction {
        return Instruction.init(op, .{ .a3 = .{ arg0, arg1, arg2 } });
    }

    pub inline fn a2(op: Opcode, arg0: u28, arg1: u28) Instruction {
        return Instruction.init(op, .{ .a2 = .{ arg0, arg1 } });
    }

    pub inline fn ah(op: Opcode, arg0: u32, arg1: u24) Instruction {
        return Instruction.init(op, .{ .ah = .{ arg0, arg1 } });
    }

    pub inline fn a(op: Opcode, arg0: u56) Instruction {
        return Instruction.init(op, .{ .a = arg0 });
    }

    pub inline fn n(op: Opcode) Instruction {
        return Instruction.init(op, .{ .n = .{} });
    }
};

const VM = struct {
    const HALT = Instruction.n(Opcode.Halt);
    const NO_PROGRAM = [_]Instruction{};
    const NUM_REGISTERS = 36;

    allocator: std.mem.Allocator,

    program: []const Instruction,
    ip: []const Instruction,

    registers: []Datum,
    constants: []Datum,
    globals: std.StringHashMap(Datum),

    pub fn init(allocator: std.mem.Allocator) !VM {
        const registers = try allocator.alloc(
            Datum,
            NUM_REGISTERS,
        );

        return VM{
            .allocator = allocator,

            .program = &NO_PROGRAM,
            .ip = &NO_PROGRAM,

            .registers = registers,
            .constants = undefined,
            .globals = std.StringHashMap(Datum).init(allocator),
        };
    }

    pub fn deinit(self: *VM) void {
        self.allocator.free(self.registers);
    }

    pub fn execute(self: *VM) void {
        self.ip = self.program;

        main: while (true) {
            switch (self.ip[0].op) {
                Opcode.Halt => break :main,
                Opcode.Noop => self.op_noop(),
                Opcode.Half => self.op_half(),
                Opcode.JumpEq => self.op_jumpeq(),
                Opcode.JumpNeq => self.op_jumpneq(),
                Opcode.Add => self.op_add(),
                Opcode.Debug => self.op_debug(),
                else => break :main,
            }

            self.ip = self.ip[1..];
        }
    }

    inline fn fetch(self: *VM) Instruction {
        return self.ip[0];
    }

    inline fn fetch_op(self: *VM) Opcode {
        return self.fetch().op;
    }

    inline fn fetch_args(self: *VM) Instruction.Args {
        return self.fetch().args;
    }

    inline fn op_noop(_: *VM) void {}

    inline fn op_half(self: *VM) void {
        const args = self.fetch_args().ah;
        const h = args[0];
        const r = args[1];

        self.registers[r].one = h;
    }

    inline fn op_jumpeq(self: *VM) void {
        const args = self.fetch_args().a3;
        const a = self.registers[args[0]].one;
        const b = self.registers[args[1]].one;
        const j = args[2];

        if (a == b) {
            self.ip = self.program[j - 1 ..];
        }
    }

    inline fn op_jumpneq(self: *VM) void {
        const args = self.fetch_args().a3;
        const a = self.registers[args[0]].one;
        const b = self.registers[args[1]].one;
        const j = args[2];

        if (a != b) {
            self.ip = self.program[j - 1 ..];
        }
    }

    inline fn op_add(self: *VM) void {
        const args = self.fetch_args().a3;
        const a = self.registers[args[0]].one;
        const b = self.registers[args[1]].one;

        self.registers[args[2]].one = a + b;
    }

    inline fn op_debug(self: *VM) void {
        const arg = self.fetch_args().a;
        const a = self.registers[arg].one;

        std.debug.print("debug: {}\n", .{a});
    }
};

test "tail call vs switch benchmarking" {
    const max: u32 = 1000;

    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();

    const allocator = gpa.allocator();

    var vm = try VM.init(allocator);
    defer vm.deinit();

    const program = try allocator.alloc(Instruction, 8);
    defer allocator.free(program);

    program[0..8].* = .{
        Instruction.ah(.Half, 1, 0),
        Instruction.ah(.Half, 1, 1),
        Instruction.ah(.Half, max, 2),
        Instruction.ah(.Half, 4, 3),
        Instruction.a3(.Add, 0, 1, 1),
        Instruction.a3(.JumpNeq, 1, 2, 3),
        Instruction.a(.Debug, 1),
        VM.HALT,
    };

    vm.program = program;

    const time = std.time;

    var timer = time.nanoTimestamp();
    vm.execute();
    const elapsed_switch = time.nanoTimestamp() - timer;

    timer = time.nanoTimestamp();
    var sum: u32 = 0;
    for (0..max) |_| {
        sum += 1;
    }
    std.debug.print("debug: {}\n", .{sum});
    const elapsed_base = time.nanoTimestamp() - timer;

    std.debug.print("switch took: {}, base took: {}\n", .{ elapsed_switch, elapsed_base });
}
