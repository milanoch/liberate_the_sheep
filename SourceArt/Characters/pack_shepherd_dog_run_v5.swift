#!/usr/bin/env swift

// Production import only. Crops/registers generated poses without repainting
// anatomy, then restores the approved volumetric pickup/jump pixels verbatim.
// reference APPROVED_KEYS_4x7.png REFERENCE_4x4.png
// register SOURCE_4x4.png REGISTRATION.json [UNIFORM_SCALE]
// prepare SOURCE_4x4.png REGISTRATION.json APPROVED_V3_13x9.png KEYS_4x7.png JUMP.png
// finish BAKED_15x14.png APPROVED_V3_13x9.png OUTPUT_V5_15x14.png

import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers

private let cell = 256
private enum PackError: Error, CustomStringConvertible {
    case failure(String)
    var description: String { switch self { case let .failure(message): return message } }
}
private struct Anchor: Codable { let x: Double; let y: Double }
private struct Registration: Codable {
    let scale: Double
    let targetX: Double
    let targetY: Double
    let source: String
    let anchors: [Anchor]
}

private func load(_ path: String) throws -> CGImage {
    guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil),
          let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
        throw PackError.failure("Cannot read \(path)")
    }
    return image
}

private func save(_ image: CGImage, _ path: String) throws {
    guard let destination = CGImageDestinationCreateWithURL(URL(fileURLWithPath: path) as CFURL,
        UTType.png.identifier as CFString, 1, nil) else {
        throw PackError.failure("Cannot create \(path)")
    }
    CGImageDestinationAddImage(destination, image, nil)
    guard CGImageDestinationFinalize(destination) else { throw PackError.failure("Cannot save \(path)") }
}

private func crop(_ atlas: CGImage, _ index: Int, _ columns: Int) throws -> CGImage {
    guard let image = atlas.cropping(to: CGRect(x: index % columns * cell,
        y: index / columns * cell, width: cell, height: cell)) else {
        throw PackError.failure("Cannot crop approved frame \(index)")
    }
    return image
}

private func cuts(_ size: Int) -> [Int] {
    (0...4).map { Int((Double($0) * Double(size) / 4).rounded()) }
}

private func context(_ width: Int, _ height: Int, _ space: CGColorSpace) throws -> CGContext {
    guard let result = CGContext(data: nil, width: width, height: height, bitsPerComponent: 8,
        bytesPerRow: width * 4, space: space,
        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue | CGBitmapInfo.byteOrder32Big.rawValue) else {
        throw PackError.failure("Cannot allocate import canvas")
    }
    result.clear(CGRect(x: 0, y: 0, width: width, height: height))
    result.interpolationQuality = .high
    result.setBlendMode(.copy)
    return result
}

private func reference(_ arguments: [String]) throws {
    guard arguments.count == 4 else { throw PackError.failure("reference KEYS_4x7.png REFERENCE_4x4.png") }
    let source = try load(arguments[2])
    guard source.width == 4 * cell, source.height == 7 * cell,
          let image = source.cropping(to: CGRect(x: 0, y: 0, width: 4 * cell, height: 4 * cell)) else {
        throw PackError.failure("Expected registered 4x7 keyframes")
    }
    try save(image, arguments[3])
    print("Cropped unchanged sixteen-pose reference: \(arguments[3])")
}

private func register(_ arguments: [String]) throws {
    guard arguments.count == 4 || arguments.count == 5 else {
        throw PackError.failure("register SOURCE_4x4.png REGISTRATION.json [UNIFORM_SCALE]")
    }
    let source = try load(arguments[2])
    guard source.width == source.height else { throw PackError.failure("Expected square 4x4 source") }
    let scale = arguments.count == 5 ? Double(arguments[4]) : Double(cell * 4) / Double(source.width)
    guard let scale, scale > 0, scale.isFinite else { throw PackError.failure("Invalid scale") }
    let canvas = try context(source.width, source.height, CGColorSpaceCreateDeviceRGB())
    canvas.draw(source, in: CGRect(x: 0, y: 0, width: source.width, height: source.height))
    guard let bytes = canvas.data?.assumingMemoryBound(to: UInt8.self) else {
        throw PackError.failure("Cannot read muzzle coordinates")
    }
    let split = cuts(source.width)
    var anchors: [Anchor] = []
    for frame in 0..<16 {
        let left = split[frame % 4], top = split[frame / 4]
        let width = split[frame % 4 + 1] - left
        let height = split[frame / 4 + 1] - top
        var first: Int?
        for y in 0..<(height / 3) {
            var opaque = 0
            for x in (width / 4)..<(width * 3 / 4) {
                if bytes[((top + y) * source.width + left + x) * 4 + 3] >= 220 { opaque += 1 }
            }
            if opaque >= 3 { first = y; break }
        }
        guard let first else { throw PackError.failure("Cannot locate muzzle for frame \(frame)") }
        var sum = 0.0, count = 0.0
        for y in first..<min(height, first + 5) {
            for x in (width / 4)..<(width * 3 / 4) {
                if bytes[((top + y) * source.width + left + x) * 4 + 3] >= 220 {
                    sum += Double(x); count += 1
                }
            }
        }
        anchors.append(Anchor(x: sum / count, y: Double(first)))
    }
    let config = Registration(scale: scale, targetX: 128, targetY: 36,
        source: arguments[2], anchors: anchors)
    let encoder = JSONEncoder()
    encoder.outputFormatting = [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]
    try encoder.encode(config).write(to: URL(fileURLWithPath: arguments[3]))
    print("Registered sixteen muzzle anchors and uniform scale \(scale): \(arguments[3])")
}

private func prepare(_ arguments: [String]) throws {
    guard arguments.count == 7 else {
        throw PackError.failure("prepare SOURCE_4x4.png REGISTRATION.json APPROVED_V3_13x9.png KEYS_4x7.png JUMP.png")
    }
    let source = try load(arguments[2])
    let config = try JSONDecoder().decode(Registration.self,
        from: Data(contentsOf: URL(fileURLWithPath: arguments[3])))
    let approved = try load(arguments[4])
    guard source.width == source.height, approved.width == 13 * cell,
          approved.height == 9 * cell, config.anchors.count == 16,
          config.scale.isFinite, config.scale > 0, let space = approved.colorSpace else {
        throw PackError.failure("Invalid registered source or approved atlas")
    }
    let keys = try context(4 * cell, 7 * cell, space)
    let split = cuts(source.width)
    for frame in 0..<16 {
        let col = frame % 4, row = frame / 4
        let width = split[col + 1] - split[col], height = split[row + 1] - split[row]
        guard let pose = source.cropping(to: CGRect(x: split[col], y: split[row],
            width: width, height: height)) else { throw PackError.failure("Cannot crop source pose") }
        let anchor = config.anchors[frame]
        let left = Double(col * cell) + config.targetX - anchor.x * config.scale
        let top = Double(row * cell) + config.targetY - anchor.y * config.scale
        keys.saveGState()
        keys.clip(to: CGRect(x: col * cell, y: 7 * cell - (row + 1) * cell, width: cell, height: cell))
        keys.draw(pose, in: CGRect(x: left, y: Double(7 * cell) - top - Double(height) * config.scale,
            width: Double(width) * config.scale, height: Double(height) * config.scale))
        keys.restoreGState()
    }
    for action in 0..<12 {
        let index = 16 + action
        keys.draw(try crop(approved, 96 + action, 13), in: CGRect(x: index % 4 * cell,
            y: 7 * cell - (index / 4 + 1) * cell, width: cell, height: cell))
    }
    guard let image = keys.makeImage() else { throw PackError.failure("Cannot finish keyframe import") }
    try save(image, arguments[5])
    try save(crop(approved, 108, 13), arguments[6])
    print("Prepared sixteen new run keys, twelve approved grab keys and approved jump")
}

private func finish(_ arguments: [String]) throws {
    guard arguments.count == 5 else {
        throw PackError.failure("finish BAKED_15x14.png APPROVED_V3_13x9.png OUTPUT_V5_15x14.png")
    }
    let outputURL = URL(fileURLWithPath: arguments[4]).standardizedFileURL
    guard outputURL != URL(fileURLWithPath: arguments[2]).standardizedFileURL,
          outputURL != URL(fileURLWithPath: arguments[3]).standardizedFileURL else {
        throw PackError.failure("Use a separate versioned output")
    }
    let baked = try load(arguments[2]), approved = try load(arguments[3])
    guard baked.width == 15 * cell, baked.height == 14 * cell,
          approved.width == 13 * cell, approved.height == 9 * cell,
          approved.bitsPerComponent == 8, approved.bitsPerPixel == 32,
          baked.bitsPerComponent == approved.bitsPerComponent,
          baked.bitsPerPixel == approved.bitsPerPixel, baked.bitmapInfo == approved.bitmapInfo,
          let space = baked.colorSpace, let approvedData = approved.dataProvider?.data,
          let bakedData = baked.dataProvider?.data,
          let approvedPointer = CFDataGetBytePtr(approvedData),
          let bakedPointer = CFDataGetBytePtr(bakedData) else {
        throw PackError.failure("Need matching RGBA storage for baked 15x14 and approved 13x9")
    }
    let outputStride = baked.bytesPerRow, approvedStride = approved.bytesPerRow
    var pixels = Array(UnsafeBufferPointer(start: bakedPointer, count: outputStride * baked.height))
    for action in 0..<13 {
        let sourceFrame = 96 + action, targetFrame = 192 + action
        for y in 0..<cell {
            let source = (sourceFrame / 13 * cell + y) * approvedStride + sourceFrame % 13 * cell * 4
            let target = (targetFrame / 15 * cell + y) * outputStride + targetFrame % 15 * cell * 4
            pixels.replaceSubrange(target..<(target + cell * 4),
                with: UnsafeBufferPointer(start: approvedPointer + source, count: cell * 4))
        }
    }
    guard let provider = CGDataProvider(data: Data(pixels) as CFData),
          let image = CGImage(width: baked.width, height: baked.height, bitsPerComponent: 8,
            bitsPerPixel: 32, bytesPerRow: outputStride, space: space,
            bitmapInfo: baked.bitmapInfo, provider: provider, decode: nil,
            shouldInterpolate: false, intent: baked.renderingIntent) else {
        throw PackError.failure("Cannot create final atlas")
    }
    try save(image, arguments[4])
    print("Wrote \(arguments[4]): 192 run frames; approved action pixels copied unchanged to 192–204")
}

do {
    let arguments = CommandLine.arguments
    switch arguments.dropFirst().first {
    case "reference": try reference(arguments)
    case "register": try register(arguments)
    case "prepare": try prepare(arguments)
    case "finish": try finish(arguments)
    default: throw PackError.failure("Choose reference, register, prepare or finish")
    }
} catch { fputs("\(error)\n", stderr); exit(1) }
