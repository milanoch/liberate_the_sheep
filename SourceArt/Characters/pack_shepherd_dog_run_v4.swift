#!/usr/bin/env swift

// Asset production only: register the generated run poses, then preserve the
// already approved v3 pickup/jump pixels when combining the completed bake.
// No anatomy deformation, repainting, preview or game execution.
//
// prepare SOURCE_4x4.png REGISTRATION.json APPROVED_V3_13x9.png KEYS_4x7.png JUMP.png
// finish BAKED_13x9.png APPROVED_V3_13x9.png OUTPUT_V4_13x9.png
// Run with swift -module-cache-path /private/tmp/shepherd-dog-v4-swift-cache.

import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers

private let cell = 256
private enum PackError: Error, CustomStringConvertible {
    case failure(String)
    var description: String { switch self { case let .failure(message): return message } }
}
private struct Anchor: Decodable {
    let x: Double
    let y: Double
    let sourcePath: String?
    let sourceColumns: Int?
    let sourceFrame: Int?
    let scale: Double?
    let cropBottom: Int?
}
private struct Registration: Decodable {
    let scale: Double
    let targetX: Double?
    let targetY: Double?
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
    guard CGImageDestinationFinalize(destination) else {
        throw PackError.failure("Cannot save \(path)")
    }
}

private func context(_ width: Int, _ height: Int, _ space: CGColorSpace) throws -> CGContext {
    guard let result = CGContext(data: nil, width: width, height: height,
        bitsPerComponent: 8, bytesPerRow: width * 4, space: space,
        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue | CGBitmapInfo.byteOrder32Big.rawValue) else {
        throw PackError.failure("Cannot allocate packed keyframes")
    }
    result.clear(CGRect(x: 0, y: 0, width: width, height: height))
    result.interpolationQuality = .high
    result.setBlendMode(.copy)
    return result
}

private func crop(_ atlas: CGImage, _ index: Int, _ columns: Int) throws -> CGImage {
    guard let result = atlas.cropping(to: CGRect(x: index % columns * cell,
        y: index / columns * cell, width: cell, height: cell)) else {
        throw PackError.failure("Cannot crop approved pose \(index)")
    }
    return result
}

private func prepare(_ arguments: [String]) throws {
    guard arguments.count == 7 else {
        throw PackError.failure("prepare SOURCE_4x4.png REGISTRATION.json APPROVED_V3_13x9.png KEYS_4x7.png JUMP.png")
    }
    let source = try load(arguments[2])
    var sources: [String: CGImage] = [arguments[2]: source]
    let config = try JSONDecoder().decode(Registration.self,
        from: Data(contentsOf: URL(fileURLWithPath: arguments[3])))
    let approved = try load(arguments[4])
    guard source.width == source.height, source.width >= 4,
          approved.width == 13 * cell, approved.height == 9 * cell,
          config.anchors.count == 16, config.scale.isFinite, config.scale > 0,
          let space = approved.colorSpace else {
        throw PackError.failure("Need a square 4x4 run source, sixteen anchors and the approved 13x9 atlas")
    }
    let keys = try context(4 * cell, 7 * cell, space)
    let targetX = config.targetX ?? 128
    let targetY = config.targetY ?? 36
    guard targetX.isFinite, targetY.isFinite else { throw PackError.failure("Invalid target anchor") }
    for frame in 0..<16 {
        let col = frame % 4, row = frame / 4
        let anchor = config.anchors[frame]
        guard anchor.x.isFinite, anchor.y.isFinite else { throw PackError.failure("Invalid muzzle anchor") }
        let sourcePath = anchor.sourcePath ?? arguments[2]
        if sources[sourcePath] == nil { sources[sourcePath] = try load(sourcePath) }
        let poseSource = sources[sourcePath]!
        let sourceColumns = anchor.sourceColumns ?? 4
        let sourceFrame = anchor.sourceFrame ?? frame
        let scale = anchor.scale ?? config.scale
        guard sourceColumns > 0, poseSource.width == poseSource.height,
              (0..<(sourceColumns * sourceColumns)).contains(sourceFrame),
              scale.isFinite, scale > 0 else { throw PackError.failure("Invalid secondary source configuration") }
        let cuts = (0...sourceColumns).map {
            Int((Double($0) * Double(poseSource.width) / Double(sourceColumns)).rounded())
        }
        let sx = sourceFrame % sourceColumns, sy = sourceFrame / sourceColumns
        let width = cuts[sx + 1] - cuts[sx]
        let cropBottom = anchor.cropBottom ?? 0
        let height = cuts[sy + 1] - cuts[sy] - cropBottom
        guard cropBottom >= 0, height > 0 else { throw PackError.failure("Invalid source-cell bottom crop") }
        guard let sourcePose = poseSource.cropping(to: CGRect(x: cuts[sx], y: cuts[sy],
            width: width, height: height)) else { throw PackError.failure("Cannot crop run key \(frame)") }
        let left = Double(col * cell) + targetX - anchor.x * scale
        let top = Double(row * cell) + targetY - anchor.y * scale
        keys.saveGState()
        keys.clip(to: CGRect(x: col * cell, y: 7 * cell - (row + 1) * cell, width: cell, height: cell))
        keys.draw(sourcePose, in: CGRect(x: left,
            y: Double(7 * cell) - top - Double(height) * scale,
            width: Double(width) * scale, height: Double(height) * scale))
        keys.restoreGState()
    }
    for action in 0..<12 {
        let index = 16 + action
        keys.draw(try crop(approved, 96 + action, 13), in: CGRect(
            x: index % 4 * cell, y: 7 * cell - (index / 4 + 1) * cell, width: cell, height: cell))
    }
    guard let image = keys.makeImage() else { throw PackError.failure("Cannot finish packed keyframes") }
    try save(image, arguments[5])
    try save(crop(approved, 108, 13), arguments[6])
    print("Prepared \(arguments[5]) and the approved v3 airborne pose \(arguments[6])")
}

private func finish(_ arguments: [String]) throws {
    guard arguments.count == 5 else {
        throw PackError.failure("finish BAKED_13x9.png APPROVED_V3_13x9.png OUTPUT_V4_13x9.png")
    }
    let outputURL = URL(fileURLWithPath: arguments[4]).standardizedFileURL
    guard outputURL != URL(fileURLWithPath: arguments[2]).standardizedFileURL,
          outputURL != URL(fileURLWithPath: arguments[3]).standardizedFileURL else {
        throw PackError.failure("Use a separate versioned output")
    }
    let baked = try load(arguments[2])
    let approved = try load(arguments[3])
    guard approved.width == 13 * cell, approved.height == 9 * cell,
          baked.width == approved.width, baked.height == approved.height,
          approved.bitsPerComponent == 8, approved.bitsPerPixel == 32,
          baked.bitsPerComponent == approved.bitsPerComponent,
          baked.bitsPerPixel == approved.bitsPerPixel,
          baked.bitmapInfo == approved.bitmapInfo,
          let space = approved.colorSpace,
          let approvedData = approved.dataProvider?.data,
          let bakedData = baked.dataProvider?.data,
          let approvedPointer = CFDataGetBytePtr(approvedData),
          let bakedPointer = CFDataGetBytePtr(bakedData) else {
        throw PackError.failure("Expected two 13x9 RGBA atlases with identical decoded pixel layout")
    }
    let outputStride = approved.bytesPerRow, sourceStride = baked.bytesPerRow
    guard CFDataGetLength(approvedData) >= outputStride * approved.height,
          CFDataGetLength(bakedData) >= sourceStride * baked.height else {
        throw PackError.failure("Incomplete PNG storage")
    }
    // Start from the approved PNG's decoded storage. Pickup and jump cells are
    // never drawn, premultiplied, color-converted or touched by this loop.
    var pixels = Array(UnsafeBufferPointer(start: approvedPointer,
        count: outputStride * approved.height))
    for frame in 0..<96 {
        let left = frame % 13 * cell, top = frame / 13 * cell
        for y in 0..<cell {
            let target = (top + y) * outputStride + left * 4
            let source = (top + y) * sourceStride + left * 4
            pixels.replaceSubrange(target..<(target + cell * 4),
                with: UnsafeBufferPointer(start: bakedPointer + source, count: cell * 4))
        }
    }
    guard let provider = CGDataProvider(data: Data(pixels) as CFData),
          let image = CGImage(width: approved.width, height: approved.height,
            bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: outputStride,
            space: space, bitmapInfo: approved.bitmapInfo, provider: provider,
            decode: nil, shouldInterpolate: false, intent: approved.renderingIntent) else {
        throw PackError.failure("Cannot create final atlas")
    }
    try save(image, arguments[4])
    print("Wrote \(arguments[4]): 96 new run cells; original v3 actions 96–108 retained by direct byte copy")
}

do {
    let arguments = CommandLine.arguments
    switch arguments.dropFirst().first {
    case "prepare": try prepare(arguments)
    case "finish": try finish(arguments)
    default: throw PackError.failure("Choose prepare or finish; see script header for arguments")
    }
} catch { fputs("\(error)\n", stderr); exit(1) }
