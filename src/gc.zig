const std = @import("std");
const any = @import("any.zig");
const Block = any.Block;
const Meta = any.Meta;
const Object = any.Object;

pub const GC = struct {
    const Error = error{
        OutOfMemory,
        NotGCObject,
    };

    const DEFAULT_STACK_SIZE = 4096;
    const DEFAULT_HEAP_SIZE = 65536;

    allocator: std.mem.Allocator,

    stack: []u8,
    heap: []u8,
    copy: []u8,

    sp: []u8,
    hp: []u8,

    pub fn init(allocator: std.mem.Allocator) !GC {
        const stack = try allocator.alloc(u8, DEFAULT_STACK_SIZE);
        errdefer allocator.free(stack);

        const heap = try allocator.alloc(u8, DEFAULT_HEAP_SIZE);
        errdefer allocator.free(heap);

        const copy = try allocator.alloc(u8, DEFAULT_HEAP_SIZE);
        errdefer allocator.free(copy);

        return GC{
            .allocator = allocator,

            .stack = stack,
            .heap = heap,
            .copy = copy,

            .sp = stack,
            .hp = heap,
        };
    }

    pub fn deinit(self: *GC) void {
        self.allocator.free(self.stack);
        self.allocator.free(self.heap);
        self.allocator.free(self.copy);
    }

    pub fn alloc(self: *GC, size: usize) ![]u8 {
        return self.stackAlloc(size) catch self.heapAlloc(size) catch {
            self.collect();

            return self.heapAlloc(size);
        };
    }

    pub fn create(self: *GC, comptime T: type) !*Block {
        var meta = Meta.reflect(T);

        if (meta) |m| {
            return Block.init(self, m.tag, m.size, m.many);
        } else {
            return Error.NotGCObject;
        }
    }

    fn collect(self: GC) void {
        _ = self; // autofix
    }

    fn stackAlloc(self: *GC, size: usize) ![]u8 {
        return anyAlloc(&self.sp, size);
    }

    fn heapAlloc(self: *GC, size: usize) ![]u8 {
        return anyAlloc(&self.hp, size);
    }

    fn anyAlloc(memory: *[]u8, size: usize) ![]u8 {
        if (size >= memory.len) {
            return Error.OutOfMemory;
        }

        const ptr = memory.*[0..size];

        memory.ptr += size;
        memory.len -= size;

        return ptr;
    }

    fn move() void {}

    inline fn swapHeaps(self: *GC) void {
        const tmp = self.heap;

        self.heap = self.copy;
        self.copy = tmp;
        self.hp = self.heap;
    }
};

test "gc alloc" {
    const allocator = std.testing.allocator;

    var gc = try GC.init(allocator);
    defer gc.deinit();

    _ = try gc.alloc(10);
}

test "gc create" {
    const allocator = std.testing.allocator;

    var gc = try GC.init(allocator);
    defer gc.deinit();

    _ = try gc.create(*any.Pair);
}
