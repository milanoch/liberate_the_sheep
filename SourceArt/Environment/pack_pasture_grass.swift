// Packs imagegen artwork only: no procedural paint or recoloring.
// Build: swiftc -O SourceArt/Environment/pack_pasture_grass.swift -o /tmp/pack_pasture_grass
// Usage: pack_pasture_grass DENSE.png SOIL.png OUTPUT.png [OUTPUT.rgba]
// Two fully opaque source cells: index 0=dense grass, index 1=warm brown soil.
// The runtime blends these into one continuous ground image; neither is a decal.
import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

enum PackError: Error, CustomStringConvertible {
    case message(String)
    var description: String {
        switch self { case .message(let text): return text }
    }
}

func load(_ path: String) throws -> CGImage {
    guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil),
          let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
        throw PackError.message("Cannot decode \(path)")
    }
    guard image.width == image.height else {
        throw PackError.message("Expected square artwork, got \(image.width)×\(image.height): \(path)")
    }
    return image
}

func run() throws {
    guard (4...5).contains(CommandLine.arguments.count) else {
        throw PackError.message("Usage: pack_pasture_grass DENSE.png SOIL.png OUTPUT.png [OUTPUT.rgba]")
    }
    let dense = try load(CommandLine.arguments[1])
    let soil = try load(CommandLine.arguments[2])
    let cell = 1024
    let width = cell * 2
    let height = cell
    let bytesPerRow = width * 4
    let colorSpace = CGColorSpace(name: CGColorSpace.sRGB)!
    let bitmapInfo = CGImageAlphaInfo.premultipliedLast.rawValue | CGBitmapInfo.byteOrder32Big.rawValue
    guard let context = CGContext(data: nil, width: width, height: height,
                                  bitsPerComponent: 8, bytesPerRow: bytesPerRow,
                                  space: colorSpace, bitmapInfo: bitmapInfo) else {
        throw PackError.message("Cannot allocate atlas bitmap")
    }
    context.interpolationQuality = .high
    context.setBlendMode(.copy)
    context.draw(dense, in: CGRect(x: 0, y: 0, width: cell, height: cell))
    context.draw(soil, in: CGRect(x: cell, y: 0, width: cell, height: cell))

    guard let pixels = context.data?.assumingMemoryBound(to: UInt8.self) else {
        throw PackError.message("Cannot inspect atlas bitmap")
    }
    var opaque = [0, 0]
    for y in 0..<cell {
        for x in 0..<cell {
            for index in 0..<2 {
                if pixels[y * bytesPerRow + (x + index * cell) * 4 + 3] == 255 {
                    opaque[index] += 1
                }
            }
        }
    }
    guard opaque.allSatisfy({ $0 == cell * cell }) else {
        throw PackError.message("Both cells must be fully opaque; opaque counts \(opaque)")
    }

    guard let output = context.makeImage(),
          let destination = CGImageDestinationCreateWithURL(
            URL(fileURLWithPath: CommandLine.arguments[3]) as CFURL,
            UTType.png.identifier as CFString, 1, nil) else {
        throw PackError.message("Cannot create PNG destination")
    }
    CGImageDestinationAddImage(destination, output, nil)
    guard CGImageDestinationFinalize(destination) else {
        throw PackError.message("Cannot save \(CommandLine.arguments[3])")
    }
    if CommandLine.arguments.count == 5 {
        // Same tightly packed sRGB RGBA8 pixels as the PNG, row 0 at the top.
        try Data(bytes: pixels, count: bytesPerRow * height).write(
            to: URL(fileURLWithPath: CommandLine.arguments[4]))
        print("Raw RGBA: \(CommandLine.arguments[4]) (\(bytesPerRow * height) bytes)")
    }
    print("Output: \(CommandLine.arguments[3]) (\(width)×\(height), RGBA sRGB)")
    print("Grass and soil: each \(cell * cell) fully opaque pixels; no gutter or patch alpha")
}

do { try run() }
catch {
    FileHandle.standardError.write(Data("\(error)\n".utf8))
    exit(1)
}
