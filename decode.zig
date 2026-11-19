const std = @import("std");
const io = std.Io;

const Pulse = struct { high: bool, length: u32 };

const gap_us = 5000;

pub fn main(init: std.process.Init) !void {
    const file: io.File = try io.Dir.cwd().openFile(init.io, "capture.txt", .{});
    defer io.File.close(file, init.io);
    const file_length = try io.File.length(file, init.io);

    const buffer = try init.gpa.alloc(u8, file_length);
    defer init.gpa.free(buffer);
    _ = try io.File.readPositionalAll(file, init.io, buffer, 0);

    var pulses: std.ArrayList(Pulse) = .empty;
    defer pulses.deinit(init.gpa);

    var lines = std.mem.tokenizeScalar(u8, buffer, '\n');
    while (lines.next()) |line| {
        const duration = try std.fmt.parseInt(u32, std.mem.trim(u8, line[2..], " \r"), 10);
        try pulses.append(init.gpa, .{ .high = line[0] == 'H', .length = duration });
    }

    var frame_start: ?usize = null;
    for (pulses.items, 0..) |pulse, i| {
        if (pulse.high or pulse.length <= gap_us) continue;
        if (frame_start) |start| decode(pulses.items[start..i], pulse.length);
        frame_start = i + 1;
    }
}

// frame = [H, L] per bit, then the sync H; gap = the sync L that ended it.
fn decode(frame: []const Pulse, gap: u32) void {
    if (frame.len % 2 == 0) return;
    const bits: u32 = @intCast(frame.len / 2);
    if (bits < 8 or bits > 32) return;

    var code: u32 = 0;
    var total_us: u32 = 0;
    var i: usize = 0;
    while (i < frame.len - 1) : (i += 2) {
        const high = frame[i];
        const low = frame[i + 1];
        if (!high.high or low.high) return;
        const short = @min(high.length, low.length);
        const long = @max(high.length, low.length);
        if (short == 0 or long < 2 * short or long > 5 * short) return;
        code = (code << 1) | @intFromBool(high.length > low.length);
        total_us += high.length + low.length;
    }

    const unit = total_us / (4 * bits);
    std.debug.print("{d} bits  code {d} = {b:0>24}  unit {d} us  sync gap = {d} units\n", .{ bits, code, code, unit, gap / unit });
}
