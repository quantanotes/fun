const GC = @import("gc.zig").GC;
const any = @import("any.zig");
const Value = any.Value;
const Object = any.Object;
const Reference = any.Reference;

pub const Symbols = struct {
    const DEFAULT_SYMBOLS_SIZE = 4096;

    gc: *GC,

    symbols: Reference,

    pub fn init(gc: *GC) !Symbols {
        const block = try gc.create([DEFAULT_SYMBOLS_SIZE]Reference);
        const symbols = block.ref();

        return Symbols{
            .gc = gc,
            .symbols = symbols,
        };
    }

    pub fn intern(self: *Symbols, data: []const u8) Value {
        _ = self; // autofix
        _ = data; // autofix
    }
};
