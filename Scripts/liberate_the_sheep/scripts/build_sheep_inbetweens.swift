#!/usr/bin/env swift

// Offline in-betweening of the original, complete painted sheep sprites.
// No body-part rig, extra legs, runtime meshes, or runtime generated textures.
// Requires macOS 13+ (Vision optical flow revision 2). Compile with swiftc -O.
// Each original run pose remains exact. Vision estimates bidirectional motion;
// interpolated signed-distance contours keep thin legs opaque rather than
// cross-fading two silhouettes. Dominant-source color avoids doubled facial
// details. A single white-coat flow is reused for all three original palettes.
import CoreGraphics
import CoreVideo
import Foundation
import ImageIO
import UniformTypeIdentifiers
import Vision

private let cell = 192
private let runOrder = [0, 8, 6, 5, 11, 4, 10, 15, 1, 2, 13, 9, 3, 12, 7, 14]
private let jumpOrder = [0, 1, 2, 3, 4, 5, 6, 11, 7, 14, 13, 10, 9, 8, 12, 16, 17, 21, 22, 15, 18, 19, 20, 23]

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
            let image = CGImage(width: width, height: height, bitsPerComponent: 8,
                bitsPerPixel: 32, bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
                bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue)
                    .union(.byteOrder32Big), provider: provider, decode: nil,
                shouldInterpolate: false, intent: .defaultIntent)
        else { throw BakeError.failure("Cannot create CGImage") }
        return image
    }

    func subcell(_ index: Int) -> Raster {
        var result = Raster(width: cell, height: cell)
        let left = (index % 16) * cell
        let top = (index / 16) * cell
        for y in 0..<cell {
            let start = ((top + y) * width + left) * 4
            result.rgba.replaceSubrange(y * cell * 4..<(y + 1) * cell * 4,
                with: rgba[start..<start + cell * 4])
        }
        return result
    }

    mutating func insert(_ other: Raster, x: Int, y: Int) {
        for row in 0..<other.height {
            let start = ((y + row) * width + x) * 4
            rgba.replaceSubrange(start..<start + other.width * 4,
                with: other.rgba[row * other.width * 4..<(row + 1) * other.width * 4])
        }
    }

    func sample(_ x: Float, _ y: Float, _ channel: Int) -> Float {
        if x < 0 || y < 0 || x > Float(width - 1) || y > Float(height - 1) { return 0 }
        let left = Int(x), top = Int(y)
        let right = min(width - 1, left + 1), bottom = min(height - 1, top + 1)
        let fx = x - Float(left), fy = y - Float(top)
        func value(_ px: Int, _ py: Int) -> Float { Float(rgba[(py * width + px) * 4 + channel]) }
        return (value(left, top) * (1 - fx) + value(right, top) * fx) * (1 - fy)
            + (value(left, bottom) * (1 - fx) + value(right, bottom) * fx) * fy
    }

    func opaqueForFlow() -> Raster {
        var result = self
        let background: [Float] = [122, 156, 168]
        for index in stride(from: 0, to: rgba.count, by: 4) {
            let remaining = 1 - Float(rgba[index + 3]) / 255
            for c in 0..<3 {
                result.rgba[index + c] = UInt8(min(255, Float(rgba[index + c]) + background[c] * remaining))
            }
            result.rgba[index + 3] = 255
        }
        return result
    }
}

private enum BakeError: Error, CustomStringConvertible {
    case failure(String)
    var description: String { switch self { case let .failure(value): return value } }
}

private func read(_ path: String) throws -> Raster {
    guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil),
          let image = CGImageSourceCreateImageAtIndex(source, 0, nil)
    else { throw BakeError.failure("Cannot load \(path)") }
    var result = Raster(width: image.width, height: image.height)
    let drawn = result.rgba.withUnsafeMutableBytes { bytes -> Bool in
        guard let context = CGContext(data: bytes.baseAddress, width: image.width, height: image.height,
            bitsPerComponent: 8, bytesPerRow: image.width * 4, space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: CGBitmapInfo.byteOrder32Big.rawValue | CGImageAlphaInfo.premultipliedLast.rawValue)
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

private func writePreviews(_ atlas: Raster, directory: String) throws {
    var coats = Raster(width: cell * 4, height: cell * 3)
    for coat in 0..<3 {
        for column in 0..<4 {
            coats.insert(atlas.subcell(coat * 80 + column * 12).opaqueForFlow(),
                x: column * cell, y: coat * cell)
        }
    }
    try write(coats, directory + "/coats.png")
    guard let destination = CGImageDestinationCreateWithURL(
        URL(fileURLWithPath: directory + "/run-48-frames.gif") as CFURL,
        UTType.gif.identifier as CFString, 48, nil)
    else { throw BakeError.failure("Cannot create animation preview") }
    CGImageDestinationSetProperties(destination, [
        kCGImagePropertyGIFDictionary: [kCGImagePropertyGIFLoopCount: 0]
    ] as CFDictionary)
    for pose in 0..<48 {
        var image = Raster(width: cell * 3, height: cell)
        for coat in 0..<3 {
            image.insert(atlas.subcell(coat * 80 + pose).opaqueForFlow(), x: coat * cell, y: 0)
        }
        // GIF stores centiseconds. Cumulative rounding preserves a 1.00s stride
        // without making any individual frame faster than the 0.02s GIF minimum.
        let start = Int((Double(pose) * 100 / 48).rounded())
        let end = Int((Double(pose + 1) * 100 / 48).rounded())
        let delay = Double(end - start) / 100
        CGImageDestinationAddImage(destination, try image.image(), [
            kCGImagePropertyGIFDictionary: [kCGImagePropertyGIFDelayTime: delay,
                kCGImagePropertyGIFUnclampedDelayTime: delay]
        ] as CFDictionary)
    }
    guard CGImageDestinationFinalize(destination) else { throw BakeError.failure("Cannot write animation preview") }
}

private struct Flow {
    let width: Int
    let height: Int
    var vectors: [SIMD2<Float>]

    func sample(_ x: Float, _ y: Float) -> SIMD2<Float> {
        let sx = max(0, min(Float(width - 1), x)), sy = max(0, min(Float(height - 1), y))
        let left = Int(sx), top = Int(sy)
        let right = min(width - 1, left + 1), bottom = min(height - 1, top + 1)
        let fx = sx - Float(left), fy = sy - Float(top)
        return (vectors[top * width + left] * (1 - fx) + vectors[top * width + right] * fx) * (1 - fy)
            + (vectors[bottom * width + left] * (1 - fx) + vectors[bottom * width + right] * fx) * fy
    }
}

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
            for y in 0..<height { for x in 0..<width {
                if (raster.rgba[(y * width + x) * 4 + 3] >= 128) == toInside { nearest[y * width + x] = SIMD2(x, y) }
            } }
            func improve(_ x: Int, _ y: Int, _ nx: Int, _ ny: Int) {
                guard nx >= 0 && nx < width && ny >= 0 && ny < height else { return }
                let index = y * width + x
                let candidate = nearest[ny * width + nx]
                let old = nearest[index]
                let d = SIMD2(x, y) &- candidate, oldD = SIMD2(x, y) &- old
                if d.x * d.x + d.y * d.y < oldD.x * oldD.x + oldD.y * oldD.y { nearest[index] = candidate }
            }
            for _ in 0..<2 {
                for y in 0..<height { for x in 0..<width {
                    improve(x, y, x - 1, y); improve(x, y, x - 1, y - 1)
                    improve(x, y, x, y - 1); improve(x, y, x + 1, y - 1)
                } }
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
        if x < 0 || y < 0 || x > Float(width - 1) || y > Float(height - 1) { return -100 }
        let left = Int(x), top = Int(y)
        let right = min(width - 1, left + 1), bottom = min(height - 1, top + 1)
        let fx = x - Float(left), fy = y - Float(top)
        return (distance[top * width + left] * (1 - fx) + distance[top * width + right] * fx) * (1 - fy)
            + (distance[bottom * width + left] * (1 - fx) + distance[bottom * width + right] * fx) * fy
    }
}

private func opticalFlow(_ first: Raster, _ second: Raster) throws -> Flow {
    try autoreleasepool {
        let request = VNGenerateOpticalFlowRequest(targetedCGImage: try second.opaqueForFlow().image(), options: [:])
        request.revision = VNGenerateOpticalFlowRequestRevision2
        request.computationAccuracy = .veryHigh
        request.outputPixelFormat = kCVPixelFormatType_TwoComponent32Float
        let handler = VNImageRequestHandler(cgImage: try first.opaqueForFlow().image(), options: [:])
        try handler.perform([request])
        guard let buffer = request.results?.first?.pixelBuffer else { throw BakeError.failure("Vision produced no optical flow") }
        CVPixelBufferLockBaseAddress(buffer, .readOnly)
        defer { CVPixelBufferUnlockBaseAddress(buffer, .readOnly) }
        guard let base = CVPixelBufferGetBaseAddress(buffer) else { throw BakeError.failure("Vision flow buffer is inaccessible") }
        let width = CVPixelBufferGetWidth(buffer), height = CVPixelBufferGetHeight(buffer)
        let rowBytes = CVPixelBufferGetBytesPerRow(buffer)
        var vectors = [SIMD2<Float>](repeating: .zero, count: width * height)
        for y in 0..<height {
            let row = base.advanced(by: y * rowBytes).assumingMemoryBound(to: Float.self)
            for x in 0..<width {
                let vector = SIMD2(row[x * 2], row[x * 2 + 1])
                guard vector.x.isFinite && vector.y.isFinite else {
                    throw BakeError.failure("Vision returned a non-finite displacement")
                }
                vectors[y * width + x] = vector
            }
        }
        return Flow(width: width, height: height, vectors: vectors)
    }
}

private func validate(_ output: Raster, original: Raster) throws {
    var left = cell, top = cell, right = 0, bottom = 0
    for coat in 0..<3 {
        var distinct = Set<Data>()
        let standing = output.subcell(coat * 80).rgba
        guard output.subcell(coat * 80 + 48).rgba == standing,
              output.subcell(coat * 80 + 71).rgba == standing else {
            throw BakeError.failure("Jump first/last poses do not match the preserved run contact pose")
        }
        for pose in 0..<72 {
            let frame = output.subcell(coat * 80 + pose)
            if pose < 48 {
                distinct.insert(Data(frame.rgba))
                if pose % 3 == 0 {
                    guard frame.rgba == original.subcell(coat * 48 + runOrder[pose / 3]).rgba else {
                        throw BakeError.failure("Original run pose \(pose / 3), coat \(coat) was altered")
                    }
                }
            } else {
                guard frame.rgba == original.subcell(coat * 48 + 16 + jumpOrder[pose - 48]).rgba else {
                    throw BakeError.failure("Original jump pose \(pose - 48), coat \(coat) was altered")
                }
            }
            let white = output.subcell(pose)
            for y in 0..<cell { for x in 0..<cell {
                let index = (y * cell + x) * 4 + 3
                if frame.rgba[index] > 8 {
                    left = min(left, x); right = max(right, x)
                    top = min(top, y); bottom = max(bottom, y)
                }
                if x < 4 || y < 4 || x >= cell - 4 || y >= cell - 4 {
                    guard frame.rgba[index] == 0 else { throw BakeError.failure("Visible pixels touch an atlas-cell border") }
                }
                guard frame.rgba[index] == white.rgba[index] else {
                    throw BakeError.failure("Coat \(coat), pose \(pose) changes silhouette")
                }
            } }
        }
        guard distinct.count == 48 else { throw BakeError.failure("Expected 48 distinct run poses; found \(distinct.count)") }
        for unused in 72..<80 {
            guard output.subcell(coat * 80 + unused).rgba.allSatisfy({ $0 == 0 }) else {
                throw BakeError.failure("Atlas padding is not transparent")
            }
        }
    }
    print("Verified: 48 distinct run frames per coat; all 16 source run / 24 jump poses exact; jump contact seams exact; shared coat silhouettes; transparent borders/padding")
    print("Visible alpha envelope (alpha > 8): x=\(left)...\(right), y=\(top)...\(bottom) in \(cell)px cell")
}

private func interpolate(_ first: Raster, _ second: Raster, forward: Flow, backward: Flow, t: Float) -> Raster {
    var result = Raster(width: first.width, height: first.height)
    let firstMatte = Matte(first), secondMatte = Matte(second)
    for y in 0..<first.height {
        for x in 0..<first.width {
            let target = SIMD2<Float>(Float(x), Float(y))
            var from = target, to = target
            // Backward sampling avoids the holes of a forward pixel splat.
            for _ in 0..<6 {
                from = target - t * forward.sample(from.x, from.y)
                to = target - (1 - t) * backward.sample(to.x, to.y)
            }
            let distance = (1 - t) * firstMatte.sample(from.x, from.y) + t * secondMatte.sample(to.x, to.y)
            let opacity = max(0, min(1, distance + 0.5))
            let firstAlpha = first.sample(from.x, from.y, 3)
            let secondAlpha = second.sample(to.x, to.y, 3)
            // Keep one painted texture sample at each pixel, not a double exposure
            // of two eyes, wool curls or hooves. Only the contour is in-betweened.
            let preferFirst = t < 0.5
            let useFirst = preferFirst ? firstAlpha > 16 || secondAlpha <= 16
                : !(secondAlpha > 16 || firstAlpha <= 16)
            let chosen = useFirst ? first : second
            let position = useFirst ? from : to
            let coverage = useFirst ? firstAlpha : secondAlpha
            if coverage > 0 && opacity > 0 {
                for channel in 0..<3 {
                    let value = chosen.sample(position.x, position.y, channel) / coverage * 255 * opacity
                    result.rgba[(y * first.width + x) * 4 + channel] = UInt8(max(0, min(255, value.rounded())))
                }
                result.rgba[(y * first.width + x) * 4 + 3] = UInt8((opacity * 255).rounded())
            }
        }
    }
    return result
}

private func run() throws {
    let arguments = CommandLine.arguments
    if arguments.count == 2 && arguments[1] == "--self-test" {
        var first = Raster(width: cell, height: cell)
        var second = first
        for y in 48..<144 { for x in 48..<144 {
            for c in 0..<3 {
                let value = UInt8(35 + (x * 17 + y * 19 + c * 31) % 180)
                first.rgba[(y * cell + x) * 4 + c] = value
                second.rgba[((y + 5) * cell + x + 12) * 4 + c] = value
            }
            first.rgba[(y * cell + x) * 4 + 3] = 255
            second.rgba[((y + 5) * cell + x + 12) * 4 + 3] = 255
        } }
        let flow = try opticalFlow(first, second)
        let actual = flow.sample(90, 90)
        guard abs(actual.x - 12) < 2 && abs(actual.y - 5) < 2 else {
            throw BakeError.failure("Flow orientation failed: expected +12,+5; observed \(actual)")
        }
        print("Flow direction self-test passed: expected +12,+5; actual \(actual)")
        return
    }
    if arguments.count == 4 && arguments[1] == "--validate" {
        try validate(read(arguments[3]), original: read(arguments[2]))
        return
    }
    if arguments.count == 4 && arguments[1] == "--previews" {
        try FileManager.default.createDirectory(atPath: arguments[3], withIntermediateDirectories: true)
        try writePreviews(read(arguments[2]), directory: arguments[3])
        return
    }
    guard arguments.count == 4 || arguments.count == 5 else {
        throw BakeError.failure("usage: build_sheep_inbetweens.swift OLD_ATLAS.png OUTPUT.png PREVIEW_DIRECTORY [--preview]")
    }
    let atlas = try read(arguments[1])
    guard atlas.width == 3072 && atlas.height == 1728 else { throw BakeError.failure("Expected old 16×9 atlas at 192px per cell") }
    let previewDirectory = arguments[3]
    try FileManager.default.createDirectory(atPath: previewDirectory, withIntermediateDirectories: true)
    let previewOnly = arguments.count == 5 && arguments[4] == "--preview"
    var output = Raster(width: 3072, height: 2880)
    var preview = Raster(width: cell * 4, height: cell * (previewOnly ? 3 : 16))
    let count = previewOnly ? 3 : 16
    for pose in 0..<count {
        let nextPose = (pose + 1) % 16
        let first = atlas.subcell(runOrder[pose]), second = atlas.subcell(runOrder[nextPose])
        let forward = try opticalFlow(first, second), backward = try opticalFlow(second, first)
        for coat in 0..<3 {
            let from = atlas.subcell(coat * 48 + runOrder[pose])
            let to = atlas.subcell(coat * 48 + runOrder[nextPose])
            for part in 0..<3 {
                let generated = part == 0 ? from : interpolate(from, to, forward: forward, backward: backward, t: Float(part) / 3)
                let index = coat * 80 + pose * 3 + part
                output.insert(generated, x: (index % 16) * cell, y: (index / 16) * cell)
                if coat == 0 { preview.insert(generated.opaqueForFlow(), x: part * cell, y: pose * cell) }
            }
        }
        preview.insert(second.opaqueForFlow(), x: 3 * cell, y: pose * cell)
        print("Prepared original pose \(pose + 1)/\(count) plus two motion-compensated in-betweens")
        fflush(stdout)
    }
    for coat in 0..<3 {
        for pose in 0..<24 {
            let original = atlas.subcell(coat * 48 + 16 + jumpOrder[pose])
            let index = coat * 80 + 48 + pose
            output.insert(original, x: (index % 16) * cell, y: (index / 16) * cell)
        }
    }
    try write(preview, previewDirectory + "/run-inbetweens.png")
    if !previewOnly {
        try validate(output, original: atlas)
        try write(output, arguments[2])
        try writePreviews(output, directory: previewDirectory)
    }
}

do { try run() } catch { fputs("\(error)\n", stderr); exit(1) }
