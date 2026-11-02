#!/usr/bin/env swift

// Offline animation-asset production, adapted from build_sheep_inbetweens.swift.
// Uses the newest supported Vision optical-flow revision (2 or 1).
// For a faster production bake: swiftc -O this-file.swift -o /tmp/shepherd-dog-baker
// Usage: shepherd-dog-baker PACKED_4x7.png OUTPUT.png [AIRBORNE_256.png] [--dense]
//
// Input: 256px registered cells, 16 chronological run keys followed by 12 grab
// keys. The run keys cover TWO complete strides and ONE slow tail cycle.
// Output: 96 run frames (each source key followed by five in-betweens), the 12
// unchanged grab keys at 96...107, and one unchanged airborne pose at 108.
// The airborne pose is the optional separate 256px PNG or input run key 4.
// Cells 109...116 remain transparent. The 13x9 grid is intentionally coprime
// for Panda's column/index%columns and row/index%rows atlas addressing.
// --dense produces v5: 192 run frames, actions 192...204 and five transparent
// padding cells in a 15x14 grid (3840x3584 PNG). Legacy output stays unchanged.
//
// The painted artwork supplies all anatomy and shading. Bidirectional optical
// flow moves existing pixels; a signed-distance matte moves a single opaque
// silhouette instead of dissolving between two sets of legs. No procedural rig,
// redraw, preview, test mode, or runtime texture generation is involved.

import CoreGraphics
import CoreVideo
import Foundation
import ImageIO
import UniformTypeIdentifiers
import Vision

private let cell = 256
private let sourceColumns = 4
private let sourceRows = 7
private let dense = CommandLine.arguments.contains("--dense")
private let outputColumns = dense ? 15 : 13
private let outputRows = dense ? 14 : 9
private let runKeys = 16
private let subdivisions = dense ? 12 : 6
private let runFrames = runKeys * subdivisions
private let grabFrames = 12

private enum BakeError: Error, CustomStringConvertible {
    case failure(String)
    var description: String {
        switch self { case let .failure(message): return message }
    }
}

// All working pixels use premultiplied RGBA; transparent pixels remain clear.
private struct Raster {
    let width: Int
    let height: Int
    var rgba: [UInt8]

    init(width: Int, height: Int) {
        self.width = width
        self.height = height
        rgba = .init(repeating: 0, count: width * height * 4)
    }

    func image() throws -> CGImage {
        guard let provider = CGDataProvider(data: Data(rgba) as CFData),
              let image = CGImage(width: width, height: height,
                bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: width * 4,
                space: CGColorSpaceCreateDeviceRGB(),
                bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue)
                    .union(.byteOrder32Big), provider: provider, decode: nil,
                shouldInterpolate: false, intent: .defaultIntent)
        else { throw BakeError.failure("Cannot create image") }
        return image
    }

    func sourceCell(_ index: Int) -> Raster {
        var result = Raster(width: cell, height: cell)
        let left = (index % sourceColumns) * cell
        let top = (index / sourceColumns) * cell
        for y in 0..<cell {
            let start = ((top + y) * width + left) * 4
            result.rgba.replaceSubrange(y * cell * 4..<(y + 1) * cell * 4,
                with: rgba[start..<start + cell * 4])
        }
        return result
    }

    mutating func insert(_ frame: Raster, at index: Int) {
        let left = (index % outputColumns) * cell
        let top = (index / outputColumns) * cell
        for y in 0..<cell {
            let start = ((top + y) * width + left) * 4
            rgba.replaceSubrange(start..<start + cell * 4,
                with: frame.rgba[y * cell * 4..<(y + 1) * cell * 4])
        }
    }

    func sample(_ x: Float, _ y: Float, _ channel: Int) -> Float {
        guard x >= 0, y >= 0, x <= Float(width - 1), y <= Float(height - 1) else { return 0 }
        let left = Int(x), top = Int(y)
        let right = min(width - 1, left + 1), bottom = min(height - 1, top + 1)
        let fx = x - Float(left), fy = y - Float(top)
        func value(_ px: Int, _ py: Int) -> Float {
            Float(rgba[(py * width + px) * 4 + channel])
        }
        return (value(left, top) * (1 - fx) + value(right, top) * fx) * (1 - fy)
            + (value(left, bottom) * (1 - fx) + value(right, bottom) * fx) * fy
    }

    func opaqueForFlow() -> Raster {
        var result = self
        let background: [Float] = [122, 156, 168]
        for index in stride(from: 0, to: rgba.count, by: 4) {
            let remaining = 1 - Float(rgba[index + 3]) / 255
            for channel in 0..<3 {
                result.rgba[index + channel] = UInt8(min(255,
                    Float(rgba[index + channel]) + background[channel] * remaining))
            }
            result.rgba[index + 3] = 255
        }
        return result
    }
}

private func read(_ path: String) throws -> Raster {
    guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil),
          let image = CGImageSourceCreateImageAtIndex(source, 0, nil)
    else { throw BakeError.failure("Cannot load \(path)") }
    var result = Raster(width: image.width, height: image.height)
    let drawn = result.rgba.withUnsafeMutableBytes { bytes -> Bool in
        guard let context = CGContext(data: bytes.baseAddress,
            width: image.width, height: image.height,
            bitsPerComponent: 8, bytesPerRow: image.width * 4,
            space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: CGBitmapInfo.byteOrder32Big.rawValue
                | CGImageAlphaInfo.premultipliedLast.rawValue)
        else { return false }
        context.draw(image, in: CGRect(x: 0, y: 0, width: image.width, height: image.height))
        return true
    }
    if !drawn { throw BakeError.failure("Cannot decode \(path)") }
    return result
}

private func write(_ raster: Raster, _ path: String) throws {
    guard let destination = CGImageDestinationCreateWithURL(URL(fileURLWithPath: path) as CFURL,
        UTType.png.identifier as CFString, 1, nil)
    else { throw BakeError.failure("Cannot open \(path)") }
    CGImageDestinationAddImage(destination, try raster.image(), nil)
    if !CGImageDestinationFinalize(destination) { throw BakeError.failure("Cannot write \(path)") }
}

private struct Flow {
    let width: Int
    let height: Int
    var vectors: [SIMD2<Float>]

    func sample(_ x: Float, _ y: Float) -> SIMD2<Float> {
        let sx = max(0, min(Float(width - 1), x))
        let sy = max(0, min(Float(height - 1), y))
        let left = Int(sx), top = Int(sy)
        let right = min(width - 1, left + 1), bottom = min(height - 1, top + 1)
        let fx = sx - Float(left), fy = sy - Float(top)
        return (vectors[top * width + left] * (1 - fx)
            + vectors[top * width + right] * fx) * (1 - fy)
            + (vectors[bottom * width + left] * (1 - fx)
            + vectors[bottom * width + right] * fx) * fy
    }
}

// Positive distances are inside the silhouette, negative distances outside.
private struct Matte {
    let width: Int
    let height: Int
    var distance: [Float]

    init(_ raster: Raster) {
        width = raster.width
        height = raster.height
        distance = []
        func distances(toInside: Bool) -> [Float] {
            var nearest = [SIMD2<Int>](repeating: SIMD2(-10000, -10000), count: width * height)
            for y in 0..<height {
                for x in 0..<width {
                    if (raster.rgba[(y * width + x) * 4 + 3] >= 128) == toInside {
                        nearest[y * width + x] = SIMD2(x, y)
                    }
                }
            }
            func improve(_ x: Int, _ y: Int, _ nx: Int, _ ny: Int) {
                guard nx >= 0, nx < width, ny >= 0, ny < height else { return }
                let index = y * width + x
                let candidate = nearest[ny * width + nx]
                let old = nearest[index]
                let d = SIMD2(x, y) &- candidate
                let oldD = SIMD2(x, y) &- old
                if d.x * d.x + d.y * d.y < oldD.x * oldD.x + oldD.y * oldD.y {
                    nearest[index] = candidate
                }
            }
            for _ in 0..<2 {
                for y in 0..<height {
                    for x in 0..<width {
                        improve(x, y, x - 1, y); improve(x, y, x - 1, y - 1)
                        improve(x, y, x, y - 1); improve(x, y, x + 1, y - 1)
                    }
                }
                for y in stride(from: height - 1, through: 0, by: -1) {
                    for x in stride(from: width - 1, through: 0, by: -1) {
                        improve(x, y, x + 1, y); improve(x, y, x + 1, y + 1)
                        improve(x, y, x, y + 1); improve(x, y, x - 1, y + 1)
                    }
                }
            }
            return nearest.enumerated().map { index, point in
                let d = SIMD2(index % width, index / width) &- point
                return sqrt(Float(d.x * d.x + d.y * d.y))
            }
        }
        let inside = distances(toInside: true), outside = distances(toInside: false)
        distance = zip(inside, outside).map { toInside, toOutside in
            toInside > 0 ? 0.5 - toInside : toOutside - 0.5
        }
    }

    func sample(_ x: Float, _ y: Float) -> Float {
        guard x >= 0, y >= 0, x <= Float(width - 1), y <= Float(height - 1) else { return -100 }
        let left = Int(x), top = Int(y)
        let right = min(width - 1, left + 1), bottom = min(height - 1, top + 1)
        let fx = x - Float(left), fy = y - Float(top)
        return (distance[top * width + left] * (1 - fx)
            + distance[top * width + right] * fx) * (1 - fy)
            + (distance[bottom * width + left] * (1 - fx)
            + distance[bottom * width + right] * fx) * fy
    }
}

private func opticalFlow(_ first: Raster, _ second: Raster) throws -> Flow {
    try autoreleasepool {
        let request = VNGenerateOpticalFlowRequest(
            targetedCGImage: try second.opaqueForFlow().image(), options: [:])
        request.revision = VNGenerateOpticalFlowRequest.supportedRevisions
            .contains(VNGenerateOpticalFlowRequestRevision2)
            ? VNGenerateOpticalFlowRequestRevision2 : VNGenerateOpticalFlowRequestRevision1
        request.computationAccuracy = .veryHigh
        request.outputPixelFormat = kCVPixelFormatType_TwoComponent32Float
        let handler = VNImageRequestHandler(cgImage: try first.opaqueForFlow().image(), options: [:])
        try handler.perform([request])
        guard let buffer = request.results?.first?.pixelBuffer else {
            throw BakeError.failure("Vision produced no optical flow")
        }
        CVPixelBufferLockBaseAddress(buffer, .readOnly)
        defer { CVPixelBufferUnlockBaseAddress(buffer, .readOnly) }
        guard let base = CVPixelBufferGetBaseAddress(buffer) else {
            throw BakeError.failure("Vision flow buffer is inaccessible")
        }
        let width = CVPixelBufferGetWidth(buffer), height = CVPixelBufferGetHeight(buffer)
        guard width == first.width, height == first.height else {
            throw BakeError.failure("Vision returned a flow field with incompatible dimensions")
        }
        let rowBytes = CVPixelBufferGetBytesPerRow(buffer)
        var vectors = [SIMD2<Float>](repeating: .zero, count: width * height)
        for y in 0..<height {
            let row = base.advanced(by: y * rowBytes).assumingMemoryBound(to: Float.self)
            for x in 0..<width {
                let vector = SIMD2(row[x * 2], row[x * 2 + 1])
                guard vector.x.isFinite, vector.y.isFinite else {
                    throw BakeError.failure("Vision returned a non-finite displacement")
                }
                vectors[y * width + x] = vector
            }
        }
        return Flow(width: width, height: height, vectors: vectors)
    }
}

private func interpolate(_ first: Raster, _ second: Raster,
    firstMatte: Matte, secondMatte: Matte, forward: Flow, backward: Flow, t: Float) -> Raster {
    var result = Raster(width: first.width, height: first.height)
    for y in 0..<first.height {
        for x in 0..<first.width {
            let target = SIMD2<Float>(Float(x), Float(y))
            var from = target, to = target
            // Inverse sampling avoids the holes left by forward pixel splats.
            for _ in 0..<6 {
                from = target - t * forward.sample(from.x, from.y)
                to = target - (1 - t) * backward.sample(to.x, to.y)
            }
            let distance = (1 - t) * firstMatte.sample(from.x, from.y)
                + t * secondMatte.sample(to.x, to.y)
            let opacity = max(0, min(1, distance + 0.5))
            let firstAlpha = first.sample(from.x, from.y, 3)
            let secondAlpha = second.sample(to.x, to.y, 3)
            // Select one warped texture, avoiding doubled eyes, paws and fur.
            // Only the moving contour uses both signed-distance samples.
            let useFirst = t < 0.5 ? firstAlpha > 16 || secondAlpha <= 16
                : !(secondAlpha > 16 || firstAlpha <= 16)
            let chosen = useFirst ? first : second
            let position = useFirst ? from : to
            let coverage = useFirst ? firstAlpha : secondAlpha
            if coverage > 0 && opacity > 0 {
                for channel in 0..<3 {
                    let value = chosen.sample(position.x, position.y, channel)
                        / coverage * 255 * opacity
                    result.rgba[(y * first.width + x) * 4 + channel] =
                        UInt8(max(0, min(255, value.rounded())))
                }
                result.rgba[(y * first.width + x) * 4 + 3] = UInt8((opacity * 255).rounded())
            }
        }
    }
    return result
}

private func run() throws {
    let arguments = CommandLine.arguments.filter { $0 != "--dense" }
    guard arguments.count == 3 || arguments.count == 4 else {
        throw BakeError.failure(
            "usage: build_shepherd_dog_inbetweens.swift PACKED_4x7.png OUTPUT.png [AIRBORNE_256.png] [--dense]")
    }
    let inputURL = URL(fileURLWithPath: arguments[1]).standardizedFileURL
    let outputURL = URL(fileURLWithPath: arguments[2]).standardizedFileURL
    guard inputURL != outputURL else { throw BakeError.failure("Output must not overwrite source artwork") }
    let atlas = try read(arguments[1])
    guard atlas.width == sourceColumns * cell, atlas.height == sourceRows * cell else {
        throw BakeError.failure("Expected a registered 4x7 atlas, 256px cells (1024x1792 PNG)")
    }
    let airborne = try arguments.count == 4 ? read(arguments[3]) : atlas.sourceCell(4)
    guard airborne.width == cell, airborne.height == cell else {
        throw BakeError.failure("The separate airborne pose must be a registered 256x256 PNG")
    }
    let keys = (0..<runKeys).map { atlas.sourceCell($0) }
    let mattes = keys.map { Matte($0) }
    var output = Raster(width: outputColumns * cell, height: outputRows * cell)
    for key in 0..<runKeys {
        let next = (key + 1) % runKeys
        let forward = try opticalFlow(keys[key], keys[next])
        let backward = try opticalFlow(keys[next], keys[key])
        output.insert(keys[key], at: key * subdivisions)
        for step in 1..<subdivisions {
            let frame = interpolate(keys[key], keys[next],
                firstMatte: mattes[key], secondMatte: mattes[next],
                forward: forward, backward: backward, t: Float(step) / Float(subdivisions))
            output.insert(frame, at: key * subdivisions + step)
        }
        print("Baked run key \(key + 1)/\(runKeys) and \(subdivisions - 1) in-between frames")
        fflush(stdout)
    }
    for key in 0..<grabFrames {
        output.insert(atlas.sourceCell(runKeys + key), at: runFrames + key)
    }
    output.insert(airborne, at: runFrames + grabFrames)
    try write(output, arguments[2])
    print("Wrote \(arguments[2]): \(runFrames) run frames, 12 original grab poses, one airborne pose; \(outputColumns)x\(outputRows) at 256px")
}

do { try run() } catch { fputs("\(error)\n", stderr); exit(1) }
