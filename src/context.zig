const std = @import("std");
const any = @import("any.zig");

const GC = @import("GC.zig").GC;
const Symbols = @import("symbols.zig").Symbols;
const Value = any.Value;

pub const Context = struct {
    gc: GC,
    symbols: Symbols,

    pub fn init(allocator: std.mem.Allocator) Context {
        const gc = GC.init(allocator);
        const symbols = Symbols.init();

        return Context{
            .gc = gc,
            .symbols = symbols,
        };
    }

    pub fn intern(self: *Context) Value {
        return self.symbols.intern();
    }

    pub fn cons(car: Value, cdr: Value) Value {
        _ = cdr;
        _ = car;
    }
};
