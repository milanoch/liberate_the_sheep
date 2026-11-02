#!/usr/bin/env swift

// Production import of the matching volumetric pickup and airborne artwork.
// Only crops, translates and uniformly scales generated artwork. The approved
// run pixels are copied directly from the decoded PNG storage, not redrawn.
//
// Usage:
// swift -module-cache-path /private/tmp/shepherd-dog-swift-cache \
//   SourceArt/Characters/pack_shepherd_dog_actions.swift \
//   APPROVED_13x9.png ACTIONS_4x4.png REGISTRATION.json OUTPUT_13x9.png
//
// Registration uses ONE scale for all action poses and 13 muzzle anchors in
// source-cell pixels: twelve pickup frames followed by the airborne frame.
// Example shape (supply all thirteen anchors measured from the actual artwork):
// {"scale":0.67,"targetX":128,"targetY":36,
//  "anchors":[{"x":156,"y":10,"sourceFrame":0}, ...]}
// sourceFrame defaults to the anchor index. Cells 109...116 remain transparent.

import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers

private let cell = 256
private let columns = 13
private let rows = 9
private let firstAction = 96
private let actionCount = 13

private enum ImportError: Error, CustomStringConvertible {
    case failure(String)
    var description: String {
        switch self { case let .failure(message): return message }
    }
}

private struct Anchor: Decodable {
    let x: Double
    let y: Double
    let sourceFrame: Int?
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
        throw ImportError.failure("Cannot read PNG: \(path)")
    }
    return image
}

private func run() throws {
    let arguments = CommandLine.arguments
    guard arguments.count == 5 else {
        throw ImportError.failure("Usage: pack_shepherd_dog_actions.swift APPROVED_13x9.png ACTIONS_4x4.png REGISTRATION.json OUTPUT_13x9.png")
    }
    let inputURL = URL(fileURLWithPath: arguments[1]).standardizedFileURL
    let outputURL = URL(fileURLWithPath: arguments[4]).standardizedFileURL
    guard inputURL != outputURL else {
        throw ImportError.failure("Write a versioned output first; do not overwrite the approved input")
    }
    let approved = try load(arguments[1])
    let source = try load(arguments[2])
    let config = try JSONDecoder().decode(Registration.self,
        from: Data(contentsOf: URL(fileURLWithPath: arguments[3])))
    let targetX = config.targetX ?? 128
    let targetY = config.targetY ?? 36
    guard approved.width == columns * cell, approved.height == rows * cell,
          approved.bitsPerComponent == 8, approved.bitsPerPixel == 32,
          let colorSpace = approved.colorSpace, colorSpace.model == .rgb,
          let provider = approved.dataProvider, let originalData = provider.data,
          let sourceBytes = CFDataGetBytePtr(originalData) else {
        throw ImportError.failure("Approved atlas must be a 3328x2304, 8-bit RGBA PNG")
    }
    guard source.width == source.height, source.width >= 4,
          config.scale.isFinite, config.scale > 0,
          targetX.isFinite, targetY.isFinite,
          config.anchors.count == actionCount else {
        throw ImportError.failure("Need a square 4x4 source, positive uniform scale and thirteen muzzle anchors")
    }

    // Keep the runtime image's color space, channel order and all original
    // decoded run bytes. Only the action cells are replaced. No CGContext ever
    // receives the approved run image, so it cannot resample or recolor it.
    let alpha = approved.alphaInfo
    let firstAlpha: Bool
    let premultiplied: Bool
    switch alpha {
    case .first: firstAlpha = true; premultiplied = false
    case .last: firstAlpha = false; premultiplied = false
    case .premultipliedFirst: firstAlpha = true; premultiplied = true
    case .premultipliedLast: firstAlpha = false; premultiplied = true
    default: throw ImportError.failure("Approved atlas must contain an alpha channel")
    }
    let byteOrder = approved.bitmapInfo.intersection(.byteOrderMask)
    guard byteOrder == .byteOrderDefault || byteOrder == .byteOrder32Big ||
          byteOrder == .byteOrder32Little else {
        throw ImportError.failure("Unsupported PNG pixel byte order")
    }
    let reverseChannels = byteOrder == .byteOrder32Little
    let stride = approved.bytesPerRow
    let byteCount = stride * approved.height
    guard CFDataGetLength(originalData) >= byteCount else {
        throw ImportError.failure("Incomplete decoded PNG storage")
    }
    var pixels = Array(UnsafeBufferPointer(start: sourceBytes, count: byteCount))
    let cuts = (0...4).map { Int((Double($0) * Double(source.width) / 4).rounded()) }

    for action in 0..<actionCount {
        let anchor = config.anchors[action]
        let sourceFrame = anchor.sourceFrame ?? action
        guard (0..<16).contains(sourceFrame), anchor.x.isFinite, anchor.y.isFinite else {
            throw ImportError.failure("Invalid source frame or anchor for action \(action)")
        }
        let sx = sourceFrame % 4, sy = sourceFrame / 4
        let width = cuts[sx + 1] - cuts[sx]
        let height = cuts[sy + 1] - cuts[sy]
        guard let crop = source.cropping(to: CGRect(x: cuts[sx], y: cuts[sy],
            width: width, height: height)) else {
            throw ImportError.failure("Cannot crop action \(action)")
        }
        var rendered = [UInt8](repeating: 0, count: cell * cell * 4)
        try rendered.withUnsafeMutableBytes { memory in
            let info = CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue)
                .union(.byteOrder32Big)
            guard let context = CGContext(data: memory.baseAddress, width: cell, height: cell,
                bitsPerComponent: 8, bytesPerRow: cell * 4, space: colorSpace,
                bitmapInfo: info.rawValue) else {
                throw ImportError.failure("Cannot allocate action cell")
            }
            context.interpolationQuality = .high
            context.setBlendMode(.copy)
            let left = targetX - anchor.x * config.scale
            let top = targetY - anchor.y * config.scale
            context.draw(crop, in: CGRect(x: left,
                y: Double(cell) - top - Double(height) * config.scale,
                width: Double(width) * config.scale, height: Double(height) * config.scale))
        }
        let destination = firstAction + action
        let dx = destination % columns * cell
        let dy = destination / columns * cell
        for y in 0..<cell {
            for x in 0..<cell {
                let from = (y * cell + x) * 4
                let to = (dy + y) * stride + (dx + x) * 4
                let a = rendered[from + 3]
                var rgba = [rendered[from], rendered[from + 1], rendered[from + 2], a]
                if !premultiplied {
                    for channel in 0..<3 {
                        rgba[channel] = a == 0 ? 0 : UInt8(min(255,
                            (Int(rgba[channel]) * 255 + Int(a) / 2) / Int(a)))
                    }
                }
                var ordered = firstAlpha ? [rgba[3], rgba[0], rgba[1], rgba[2]] : rgba
                if reverseChannels { ordered.reverse() }
                for channel in 0..<4 { pixels[to + channel] = ordered[channel] }
            }
        }
    }

    for empty in (firstAction + actionCount)..<(columns * rows) {
        let dx = empty % columns * cell, dy = empty / columns * cell
        for y in 0..<cell {
            let start = (dy + y) * stride + dx * 4
            pixels.replaceSubrange(start..<(start + cell * 4),
                with: repeatElement(UInt8(0), count: cell * 4))
        }
    }
    guard let outputProvider = CGDataProvider(data: Data(pixels) as CFData),
          let output = CGImage(width: approved.width, height: approved.height,
              bitsPerComponent: approved.bitsPerComponent, bitsPerPixel: approved.bitsPerPixel,
              bytesPerRow: stride, space: colorSpace, bitmapInfo: approved.bitmapInfo,
              provider: outputProvider, decode: nil, shouldInterpolate: false,
              intent: approved.renderingIntent),
          let destination = CGImageDestinationCreateWithURL(outputURL as CFURL,
              UTType.png.identifier as CFString, 1, nil) else {
        throw ImportError.failure("Cannot create output atlas")
    }
    CGImageDestinationAddImage(destination, output, nil)
    guard CGImageDestinationFinalize(destination) else {
        throw ImportError.failure("Cannot save output atlas")
    }
    print("Wrote \(arguments[4]): retained 96 run cells, imported 12 pickup poses and one airborne pose")
}

do { try run() } catch { fputs("\(error)\n", stderr); exit(1) }
