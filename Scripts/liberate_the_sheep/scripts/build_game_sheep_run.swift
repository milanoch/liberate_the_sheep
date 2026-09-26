#!/usr/bin/env swift

// Offline contact-locked trot, baked into ordinary whole-animal Panda Sprites.
// Existing white standing pose is copied, not redrawn. Standing and running
// share one coat treatment so clicking never changes fleece/ear colour.
// Hooves travel in fixed longitudinal lanes; wool/tail carry weight separately.
// 96 phases, no runtime deformation or extra entities/materials.
// Compile with swiftc -O; uses the macOS CoreGraphics raster pipeline.
import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

private let cellWidth = 256, cellHeight = 512, columns = 25, rows = 12, coats = 3
private let keyCount = 8, samplesPerKey = 12
private let runCount = keyCount * samplesPerKey
private let posesPerCoat = runCount + 1, slotsPerCoat = 100
private let previewCycleSeconds = 1.6 / 5.5
// Keep the entire standing head/collar, then join inside solid shoulder wool.
// Feet are separate layers underneath, so no stationary hoof is pasted in.
private let headJoinStart = 136, headJoinEnd = 176, fixedHeadRows = 115
private enum BakeError: Error { case failed(String) }
private struct Raster {
    let width: Int, height: Int
    var rgba: [UInt8]
    init(_ width: Int, _ height: Int) {
        self.width = width; self.height = height
        rgba = .init(repeating: 0, count: width * height * 4)
    }
    func index(_ x: Int, _ y: Int) -> Int { (y * width + x) * 4 }
    func image() throws -> CGImage {
        guard let provider = CGDataProvider(data: Data(rgba) as CFData),
            let image = CGImage(width: width, height: height, bitsPerComponent: 8,
                bitsPerPixel: 32, bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
                bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue)
                    .union(.byteOrder32Big), provider: provider, decode: nil,
                shouldInterpolate: true, intent: .defaultIntent)
        else { throw BakeError.failed("Cannot make RGBA image") }
        return image
    }
    func crop(_ left: Int, _ top: Int, _ width: Int, _ height: Int) -> Raster {
        var result = Raster(width, height)
        for y in 0..<height {
            let start = index(left, top + y)
            result.rgba.replaceSubrange(y * width * 4..<(y + 1) * width * 4,
                with: rgba[start..<start + width * 4])
        }
        return result
    }
    mutating func insert(_ cell: Raster, _ x: Int, _ y: Int) {
        for row in 0..<cell.height {
            let start = index(x, y + row)
            rgba.replaceSubrange(start..<start + cell.width * 4,
                with: cell.rgba[row * cell.width * 4..<(row + 1) * cell.width * 4])
        }
    }
    mutating func over(_ top: Raster) {
        for i in stride(from: 0, to: rgba.count, by: 4) {
            let remaining = 1 - Double(top.rgba[i + 3]) / 255
            for c in 0..<4 {
                rgba[i + c] = UInt8(min(255, (Double(top.rgba[i + c]) + Double(rgba[i + c]) * remaining).rounded()))
            }
        }
    }
    func sample(_ x: Double, _ y: Double, _ channel: Int) -> Double {
        guard x >= 0, y >= 0, x < Double(width - 1), y < Double(height - 1) else { return 0 }
        let ix = Int(x), iy = Int(y), fx = x - Double(Int(x)), fy = y - Double(Int(y))
        func value(_ px: Int, _ py: Int) -> Double { Double(rgba[index(px, py) + channel]) }
        return (value(ix, iy) * (1 - fx) + value(ix + 1, iy) * fx) * (1 - fy)
            + (value(ix, iy + 1) * (1 - fx) + value(ix + 1, iy + 1) * fx) * fy
    }
    func resized(_ width: Int, _ height: Int) -> Raster {
        var result = Raster(width, height)
        for y in 0..<height { for x in 0..<width { for c in 0..<4 {
            result.rgba[result.index(x, y) + c] = UInt8(max(0, min(255, sample(
                (Double(x) + 0.5) * Double(self.width) / Double(width) - 0.5,
                (Double(y) + 0.5) * Double(self.height) / Double(height) - 0.5, c))))
        } } }
        return result
    }
}

private func read(_ path: String) throws -> Raster {
    guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil),
        let image = CGImageSourceCreateImageAtIndex(source, 0, nil)
    else { throw BakeError.failed("Cannot read \(path)") }
    var result = Raster(image.width, image.height)
    let ok = result.rgba.withUnsafeMutableBytes { bytes -> Bool in
        guard let context = CGContext(data: bytes.baseAddress, width: image.width,
            height: image.height, bitsPerComponent: 8, bytesPerRow: image.width * 4,
            space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGBitmapInfo.byteOrder32Big.rawValue
                | CGImageAlphaInfo.premultipliedLast.rawValue) else { return false }
        context.draw(image, in: CGRect(x: 0, y: 0, width: image.width, height: image.height))
        return true
    }
    guard ok else { throw BakeError.failed("Cannot decode \(path)") }
    return result
}

private func write(_ raster: Raster, _ path: String) throws {
    guard let destination = CGImageDestinationCreateWithURL(URL(fileURLWithPath: path) as CFURL,
        UTType.png.identifier as CFString, 1, nil) else { throw BakeError.failed("Cannot write \(path)") }
    CGImageDestinationAddImage(destination, try raster.image(), nil)
    guard CGImageDestinationFinalize(destination) else { throw BakeError.failed("Cannot finish \(path)") }
}

private func neighbors(_ index: Int, _ width: Int, _ height: Int) -> [Int] {
    let x = index % width, y = index / width
    var result: [Int] = []
    if x > 0 { result.append(index - 1) }
    if x + 1 < width { result.append(index + 1) }
    if y > 0 { result.append(index - width) }
    if y + 1 < height { result.append(index + width) }
    return result
}

private func matte(_ source: Raster) throws -> Raster {
    // Imagegen twice returned a painted neutral checkerboard. Only pixels
    // connected to the cell border can be removed: enclosed fleece is protected.
    var result = source
    let count = source.width * source.height
    var candidate = [Bool](repeating: false, count: count)
    for i in 0..<count {
        let r = Int(source.rgba[i * 4]), g = Int(source.rgba[i * 4 + 1])
        let b = Int(source.rgba[i * 4 + 2]), alpha = source.rgba[i * 4 + 3]
        candidate[i] = alpha < 12 || (min(r, g, b) >= 157 && max(r, g, b) - min(r, g, b) <= 19)
    }
    var exterior = [Bool](repeating: false, count: count), queue: [Int] = []
    func enqueue(_ i: Int) {
        if candidate[i] && !exterior[i] { exterior[i] = true; queue.append(i) }
    }
    for x in 0..<source.width { enqueue(x); enqueue((source.height - 1) * source.width + x) }
    for y in 0..<source.height { enqueue(y * source.width); enqueue((y + 1) * source.width - 1) }
    var cursor = 0
    while cursor < queue.count {
        let i = queue[cursor]; cursor += 1
        for next in neighbors(i, source.width, source.height) { enqueue(next) }
    }
    var visited = exterior, largest: [Int] = []
    for start in 0..<count where !visited[start] {
        var component = [start]; visited[start] = true; cursor = 0
        while cursor < component.count {
            let i = component[cursor]; cursor += 1
            for next in neighbors(i, source.width, source.height) where !visited[next] {
                visited[next] = true; component.append(next)
            }
        }
        if component.count > largest.count { largest = component }
    }
    guard largest.count > count / 8, largest.count < count * 3 / 4 else {
        throw BakeError.failed("Unexpected sheep matte area: \(largest.count)/\(count)")
    }
    var keep = [Bool](repeating: false, count: count)
    for i in largest { keep[i] = true }
    for i in 0..<count where !keep[i] {
        for c in 0..<4 { result.rgba[i * 4 + c] = 0 }
    }
    // The source is RGB: the one-pixel antialias fringe was composited against
    // the checkerboard. Reconstruct this fringe from the nearest interior
    // colour and a soft alpha, rather than retaining white pixels on dark fur.
    var depth = [Int](repeating: 0, count: count)
    for i in largest { depth[i] = 4 }
    for ring in 1...3 {
        var boundary: [Int] = []
        for i in largest where depth[i] == 4 {
            if neighbors(i, source.width, source.height).contains(where: { depth[$0] == ring - 1 }) {
                boundary.append(i)
            }
        }
        for i in boundary { depth[i] = ring }
    }
    for i in largest where depth[i] <= 2 {
        let x = i % source.width, y = i / source.width
        var nearest = -1, distance = Int.max
        for dy in -3...3 { for dx in -3...3 {
            let nx = x + dx, ny = y + dy
            guard nx >= 0, nx < source.width, ny >= 0, ny < source.height else { continue }
            let other = ny * source.width + nx, squared = dx * dx + dy * dy
            if depth[other] >= 3 && squared < distance { nearest = other; distance = squared }
        } }
        guard nearest >= 0 else { continue }
        let coverage = depth[i] == 1 ? 0.45 : 0.88
        for c in 0..<3 { result.rgba[i * 4 + c] = UInt8(Double(source.rgba[nearest * 4 + c]) * coverage) }
        result.rgba[i * 4 + 3] = UInt8(255 * coverage)
    }
    return result
}

private struct Bounds {
    var left: Int, top: Int, right: Int, bottom: Int
    var width: Int { right - left + 1 }
    var height: Int { bottom - top + 1 }
}
private func bounds(_ raster: Raster) -> Bounds {
    var box = Bounds(left: raster.width, top: raster.height, right: 0, bottom: 0)
    for y in 0..<raster.height { for x in 0..<raster.width where raster.rgba[raster.index(x, y) + 3] > 128 {
        box.left = min(box.left, x); box.top = min(box.top, y)
        box.right = max(box.right, x); box.bottom = max(box.bottom, y)
    } }
    return box
}

private func frontCenter(_ raster: Raster) -> Double {
    let box = bounds(raster)
    // Register frames by the unchanged head/ears, not the overall silhouette.
    // A rear-only weight shift must not move the whole sheep the other way.
    let front = raster.crop(0, box.top, raster.width, max(1, box.height / 3))
    let anchor = bounds(front)
    return Double(anchor.left + anchor.right) * 0.5
}

private func fitted(_ source: Raster, sourceBox: Bounds, target: Bounds) -> Raster {
    var result = Raster(cellWidth, cellHeight)
    let scaleX = Double(target.width) / Double(sourceBox.width)
    let scaleY = Double(target.height) / Double(sourceBox.height)
    for y in 0..<cellHeight { for x in 0..<cellWidth { for c in 0..<4 {
        let sx = (Double(x - target.left) + 0.5) / scaleX + Double(sourceBox.left) - 0.5
        let sy = (Double(y - target.top) + 0.5) / scaleY + Double(sourceBox.top) - 0.5
        result.rgba[result.index(x, y) + c] = UInt8(max(0, min(255, source.sample(sx, sy, c))))
    } } }
    return result
}

private func rearCenter(_ frame: Raster) -> Double {
    var mass = 0.0, weighted = 0.0
    for y in (cellHeight * 2 / 3)..<cellHeight { for x in 0..<cellWidth {
        let alpha = Double(frame.rgba[frame.index(x, y) + 3]) / 255
        mass += alpha; weighted += Double(x) * alpha
    } }
    return weighted / max(1, mass)
}

private func recolor(_ source: Raster, _ coat: Int, completeRunFleece: Bool = false) -> Raster {
    guard coat != 0 else { return source }
    var output = source
    let count = source.width * source.height
    // Warm fleece and neutral curl highlights form one connected component.
    // Bright eye glints are isolated by the dark face and remain unchanged.
    var pale = [Bool](repeating: false, count: count)
    for i in 0..<count where source.rgba[i * 4 + 3] > 0 {
        let alpha = Double(source.rgba[i * 4 + 3]) / 255
        let r = Double(source.rgba[i * 4]) / alpha, g = Double(source.rgba[i * 4 + 1]) / alpha
        let b = Double(source.rgba[i * 4 + 2]) / alpha
        pale[i] = r >= 107 && g >= 84 && b >= 53 && r >= b && g >= b - 3 && r - g < 75
    }
    let seed = (source.height / 2) * source.width + source.width / 2
    var wool = [Bool](repeating: false, count: count), queue = [seed], cursor = 0
    wool[seed] = true
    while cursor < queue.count {
        let i = queue[cursor]; cursor += 1
        for next in neighbors(i, source.width, source.height) where pale[next] && !wool[next] {
            wool[next] = true; queue.append(next)
        }
    }
    if completeRunFleece {
        // New painted keys contain a few bright curls enclosed by a dark curl
        // outline near the rump. They are fleece, but cannot connect to the
        // centre flood fill. Include their pale pixels below the front third;
        // the face/eyes remain protected. Standing keeps its exact old pixels.
        let box = bounds(source), shoulder = box.top + box.height / 3
        for y in shoulder..<source.height { for x in 0..<source.width {
            let i = y * source.width + x
            let alpha = Double(source.rgba[i * 4 + 3]) / 255
            guard alpha > 0 else { continue }
            let rgb = (0..<3).map { Double(source.rgba[i * 4 + $0]) / alpha }
            let neutralHighlight = rgb.min()! >= 155 && rgb.max()! - rgb.min()! <= 25
            if pale[i] || neutralHighlight { wool[i] = true }
        } }
    }
    // Include the connected cream shadow fringe, not only bright highlights.
    // This is a whole-fleece mask; isolated eye glints cannot join it.
    for _ in 0..<2 {
        let previous = wool
        for i in 0..<count where !previous[i] && source.rgba[i * 4 + 3] > 0 {
            let a = Double(source.rgba[i * 4 + 3]) / 255
            let r = Double(source.rgba[i * 4]) / a, g = Double(source.rgba[i * 4 + 1]) / a
            let b = Double(source.rgba[i * 4 + 2]) / a
            if r >= 90 && g >= 70 && b >= 43 && r >= b && r - g < 80
                && neighbors(i, source.width, source.height).contains(where: { previous[$0] }) {
                wool[i] = true
            }
        }
    }
    if completeRunFleece {
        // Keep the nose/eyes and upper muzzle out of the two-pixel fleece
        // fringe expansion. Stop ABOVE the forehead wool (not a horizontal
        // white tuft stripe). Classification runs only on painted keys.
        let box = bounds(source), muzzleBottom = box.top + Int(Double(box.height) * 0.16)
        let earBottom = box.top + Int(Double(box.height) * 0.28)
        for y in 0..<earBottom { for x in 0..<source.width {
            let i = y * source.width + x, alpha = Double(source.rgba[i * 4 + 3]) / 255
            guard alpha > 0 else { continue }
            let red = Double(source.rgba[i * 4]) / alpha
            let green = Double(source.rgba[i * 4 + 1]) / alpha
            let blue = Double(source.rgba[i * 4 + 2]) / alpha
            let atEar = abs(Double(x) - Double(cellWidth - 1) * 0.5) > Double(cellWidth) * 0.15
            let pinkEar = atEar && red - green > 35 && green - blue < 45 && red - blue > 50
            if y < muzzleBottom || pinkEar { wool[i] = false }
        } }
    }
    let dark: [Double] = coat == 1 ? [31, 30, 31] : [68, 38, 21]
    let light: [Double] = coat == 1 ? [131, 129, 126] : [209, 144, 80]
    for i in 0..<count where wool[i] {
        let a = Double(source.rgba[i * 4 + 3]) / 255
        guard a > 0 else { continue }
        let r = Double(source.rgba[i * 4]) / a, g = Double(source.rgba[i * 4 + 1]) / a
        let b = Double(source.rgba[i * 4 + 2]) / a
        let luminance = 0.2126 * r + 0.7152 * g + 0.0722 * b
        // Expand the narrow cream luminance range to preserve curl shading
        // and match the established standing sprite's dark/brown coats.
        let shade = pow(max(0, min(1, (luminance - 138) / 117)), 1.8)
        for c in 0..<3 { output.rgba[i * 4 + c] = UInt8((dark[c] + (light[c] - dark[c]) * shade) * a) }
    }
    return output
}

private func anchoredStandingHead(_ runningBody: Raster, standingBody: Raster) -> Raster {
    var output = runningBody
    for y in 0..<headJoinEnd {
        let t = max(0, min(1, Double(y - headJoinStart) / Double(headJoinEnd - headJoinStart)))
        let runWeight = t * t * (3 - 2 * t)
        for x in 0..<cellWidth { for c in 0..<4 {
            let i = output.index(x, y) + c
            // Complementary premultiplied RGBA interpolation, NOT two faded
            // source-over layers: overlapping opaque wool must stay opaque.
            output.rgba[i] = UInt8((Double(standingBody.rgba[i]) * (1 - runWeight)
                + Double(runningBody.rgba[i]) * runWeight).rounded())
        } }
    }
    return output
}

private func validateHeadJoin(_ frame: Raster, standing: Raster, coat: Int, pose: Int) throws {
    let headBytes = cellWidth * fixedHeadRows * 4
    guard frame.rgba.prefix(headBytes) == standing.rgba.prefix(headBytes) else {
        throw BakeError.failed("Standing/run head colour or coverage changed: coat \(coat), pose \(pose)")
    }
    // The nose-to-shoulder core must not acquire a translucent cut. External
    // antialias edges and legitimate gaps alongside ears/legs are excluded.
    for y in 80..<200 { for x in 112..<144 {
        guard frame.rgba[frame.index(x, y) + 3] >= 240 else {
            throw BakeError.failed("Translucent neck join: coat \(coat), pose \(pose), pixel \(x),\(y)")
        }
    } }
}

// Signed-distance silhouettes are used only for offline progression checks.
private struct DistanceMatte {
    let width: Int, height: Int
    var distance: [Float]
    init(_ raster: Raster) {
        width = raster.width; height = raster.height; distance = []
        func distances(toInside: Bool) -> [Float] {
            var nearest = [SIMD2<Int>](repeating: SIMD2(-10000, -10000), count: width * height)
            for y in 0..<height { for x in 0..<width {
                if (raster.rgba[(y * width + x) * 4 + 3] >= 128) == toInside {
                    nearest[y * width + x] = SIMD2(x, y)
                }
            } }
            func improve(_ x: Int, _ y: Int, _ nx: Int, _ ny: Int) {
                guard nx >= 0, nx < width, ny >= 0, ny < height else { return }
                let index = y * width + x, candidate = nearest[ny * width + nx], old = nearest[index]
                let delta = SIMD2(x, y) &- candidate, oldDelta = SIMD2(x, y) &- old
                if delta.x * delta.x + delta.y * delta.y < oldDelta.x * oldDelta.x + oldDelta.y * oldDelta.y {
                    nearest[index] = candidate
                }
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
                let delta = SIMD2(index % width, index / width) &- point
                return sqrt(Float(delta.x * delta.x + delta.y * delta.y))
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
        return (distance[top * width + left] * (1 - fx) + distance[top * width + right] * fx) * (1 - fy)
            + (distance[bottom * width + left] * (1 - fx) + distance[bottom * width + right] * fx) * fy
    }
}

private func selfTest() throws {
    // Premultiplied source-over must retain both alpha coverage and colour,
    // including a completely transparent source pixel.
    var bottom = Raster(2, 1), top = Raster(2, 1)
    bottom.rgba = [20, 40, 60, 128, 10, 8, 6, 16]
    top.rgba = [50, 10, 5, 64, 0, 0, 0, 0]
    bottom.over(top)
    guard bottom.rgba == [65, 40, 50, 160, 10, 8, 6, 16] else {
        throw BakeError.failed("Premultiplied source-over changed colour or coverage")
    }

    // The body is textured, not a flat silhouette: a seam or front-half warp
    // must fail even if its bounding box happens to remain unchanged.
    var source = Raster(cellWidth, cellHeight)
    for y in 20..<(cellHeight - 20) { for x in 24..<(cellWidth - 24) {
        let index = source.index(x, y)
        let alpha = x == 24 || x == cellWidth - 25 ? 112 : 255
        source.rgba[index + 3] = UInt8(alpha)
        for channel in 0..<3 {
            let value = 35 + (x * 17 + y * 19 + channel * 31) % 180
            source.rgba[index + channel] = UInt8(value * alpha / 255)
        }
    } }
    let first = weightShiftedBody(source, phase: 0)
    let looped = weightShiftedBody(source, phase: 1)
    guard first.rgba == looped.rgba else {
        throw BakeError.failed("Weight transfer has a discontinuity at the loop boundary")
    }
    for phase in [0.0, 0.15, 0.3, 0.5, 0.85, 1.0] {
        let frame = weightShiftedBody(source, phase: phase)
        guard frame.rgba.prefix(cellWidth * cellHeight / 2 * 4) ==
            source.rgba.prefix(cellWidth * cellHeight / 2 * 4) else {
            throw BakeError.failed("Weight transfer moved the front half")
        }
        for index in stride(from: 0, to: frame.rgba.count, by: 4) {
            guard (0..<3).allSatisfy({ frame.rgba[index + $0] <= frame.rgba[index + 3] }) else {
                throw BakeError.failed("Weight transfer broke premultiplied alpha")
            }
        }
    }
    guard first.rgba != weightShiftedBody(source, phase: 0.5).rgba else {
        throw BakeError.failed("Weight transfer produced no animated body movement")
    }
    var standing = source
    for i in stride(from: 0, to: standing.rgba.count, by: 4) {
        for c in 0..<3 { standing.rgba[i + c] /= 2 }
    }
    let joined = anchoredStandingHead(source, standingBody: standing)
    guard joined.rgba.prefix(cellWidth * headJoinStart * 4) ==
        standing.rgba.prefix(cellWidth * headJoinStart * 4),
        joined.rgba.suffix((cellHeight - headJoinEnd) * cellWidth * 4) ==
        source.rgba.suffix((cellHeight - headJoinEnd) * cellWidth * 4) else {
        throw BakeError.failed("Head join did not preserve standing head or running body")
    }
    for i in stride(from: 0, to: joined.rgba.count, by: 4) {
        guard joined.rgba[i + 3] == source.rgba[i + 3],
            (0..<3).allSatisfy({ joined.rgba[i + $0] <= joined.rgba[i + 3] }) else {
            throw BakeError.failed("Head feather introduced an alpha dip or unassociated RGB")
        }
    }
    print("Premultiplied compositing, periodic body motion and gap-free standing-head join self-tests passed")
}

private struct HoofTrack {
    let phase: Double, stance: Bool, x: Double, y: Double, lift: Double
    let laneX: Double, neutralY: Double
}

private func readTracks(_ path: String) throws -> [[HoofTrack]] {
    let names = ["front_left", "front_right", "hind_left", "hind_right"]
    let lines = try String(contentsOfFile: path, encoding: .utf8).split(whereSeparator: \.isNewline)
    guard lines.first == "frame,leg,phase,stance,x,y,lift,lane_x,neutral_y", lines.count == runCount * 4 + 1 else {
        throw BakeError.failed("Expected 96 x 4 C++ trot samples")
    }
    var result = [[HoofTrack]](repeating: [], count: runCount)
    for (index, line) in lines.dropFirst().enumerated() {
        let fields = line.split(separator: ",")
        guard fields.count == 9, Int(fields[0]) == index / 4, fields[1] == names[index % 4],
            let phase = Double(fields[2]), let stance = Int(fields[3]),
            let x = Double(fields[4]), let y = Double(fields[5]), let lift = Double(fields[6]),
            let laneX = Double(fields[7]), let neutralY = Double(fields[8]),
            phase.isFinite, y.isFinite, x == 0, lift >= 0, lift <= 1,
            laneX.isFinite, neutralY.isFinite, stance == 0 || stance == 1 else {
            throw BakeError.failed("Invalid trot sample: \(line)")
        }
        result[index / 4].append(HoofTrack(phase: phase, stance: stance == 1, x: x, y: y, lift: lift,
            laneX: laneX, neutralY: neutralY))
    }
    return result
}

private func hoofLayers(_ master: Raster) throws -> (body: Raster, hooves: [Raster]) {
    // Derive layers from the same neutral painted sheep, not four unrelated
    // drawings. Only the four isolated grey hoof components are separated.
    let regions = [Bounds(left: 24, top: 120, right: 101, bottom: 211),
                   Bounds(left: 154, top: 120, right: 232, bottom: 211),
                   Bounds(left: 24, top: 417, right: 126, bottom: 499),
                   Bounds(left: 129, top: 417, right: 232, bottom: 499)]
    var body = master, hooves: [Raster] = []
    for (leg, region) in regions.enumerated() {
        var candidates = [Bool](repeating: false, count: cellWidth * cellHeight)
        for y in region.top...region.bottom { for x in region.left...region.right {
            let i = master.index(x, y), alpha = Double(master.rgba[i + 3]) / 255
            guard alpha > 0 else { continue }
            let rgb = (0..<3).map { Double(master.rgba[i + $0]) / alpha }
            candidates[y * cellWidth + x] = rgb.max()! < 175 && rgb[0] - rgb[2] < 30
        } }
        var visited = [Bool](repeating: false, count: candidates.count), largest: [Int] = []
        for start in candidates.indices where candidates[start] && !visited[start] {
            var component = [start], cursor = 0; visited[start] = true
            while cursor < component.count {
                let i = component[cursor]; cursor += 1
                for next in neighbors(i, cellWidth, cellHeight) where candidates[next] && !visited[next] {
                    visited[next] = true; component.append(next)
                }
            }
            if component.count > largest.count { largest = component }
        }
        guard largest.count > 70, largest.count < 1500 else {
            throw BakeError.failed("Unrecognised neutral hoof \(leg): \(largest.count) pixels")
        }
        var layer = Raster(cellWidth, cellHeight)
        for pixel in largest { for c in 0..<4 {
            layer.rgba[pixel * 4 + c] = master.rgba[pixel * 4 + c]
            body.rgba[pixel * 4 + c] = 0
        } }
        let box = bounds(layer)
        print("Neutral hoof \(leg): \(largest.count) pixels, \(box)")
        hooves.append(layer)
    }
    var restored = Raster(cellWidth, cellHeight)
    for hoof in hooves { restored.over(hoof) }
    restored.over(body)
    guard restored.rgba == master.rgba else { throw BakeError.failed("Layer extraction changed original pixels") }
    return (body, hooves)
}

private func weightShiftedBody(_ source: Raster, phase: Double) -> Raster {
    let tau = 2 * Double.pi
    // Lean over the supporting hind hoof; two soft compression/rebound beats
    // per stride, with a small delayed tail follow-through. No foot is warped.
    let support = -cos(tau * (phase - 0.15))
    let rebound = 4.5 * (1 - cos(2 * tau * (phase - 0.15)))
    var output = source
    for y in (cellHeight / 2)..<cellHeight {
        let t = max(0, min(1, (Double(y) / Double(cellHeight) - 0.5) / 0.32))
        let weight = t * t * (3 - 2 * t)
        let tailT = max(0, min(1, (Double(y) / Double(cellHeight) - 0.86) / 0.08))
        let tail = tailT * tailT * (3 - 2 * tailT)
        let dx = 9 * weight * support + 3 * tail * sin(tau * (phase - 0.24))
        let width = 1 + 0.022 * weight * cos(2 * tau * (phase - 0.15))
        for x in 0..<cellWidth {
            let side = max(-1, min(1, (Double(x) - 127.5) / 80))
            let dy = weight * (-rebound + 5 * side * support)
            let sx = 127.5 + (Double(x) - 127.5 - dx) / width
            for c in 0..<4 {
                output.rgba[output.index(x, y) + c] = UInt8(max(0, min(255,
                    source.sample(sx, Double(y) - dy, c).rounded())))
            }
        }
    }
    return output
}

private func placedHoof(_ source: Raster, track: HoofTrack, leg: Int) -> Raster {
    let box = bounds(source), sourceX = Double(box.left + box.right) / 2
    let sourceY = Double(box.top + box.bottom) / 2
    // Fixed lane widths: no lateral kicking, and no connection to wool sway.
    let lane = track.laneX + track.x
    let targetY = track.neutralY + track.y
    let scaleY = 1 - 0.18 * track.lift
    var result = Raster(cellWidth, cellHeight)
    for y in 0..<cellHeight { for x in 0..<cellWidth { for c in 0..<4 {
        result.rgba[result.index(x, y) + c] = UInt8(max(0, min(255, source.sample(
            Double(x) - lane + sourceX, (Double(y) - targetY) / scaleY + sourceY, c).rounded())))
    } } }
    if leg >= 2 && targetY > 425 {
        // A compact painted shin connects the rearward hoof to its hidden hip.
        // Most of this strip is occluded by wool. Never stretch a whole hoof
        // or swing the distal foot sideways with the pelvis.
        var shin = Raster(cellWidth, cellHeight)
        for y in 425...min(cellHeight - 1, Int(ceil(targetY))) {
            for x in max(0, Int(lane) - 7)...min(cellWidth - 1, Int(lane) + 7) {
                let edge = max(0, min(1, 6.5 - abs(Double(x) - lane)))
                let t = (Double(y) - 425) / max(1, targetY - 425)
                for c in 0..<4 {
                    shin.rgba[shin.index(x, y) + c] = UInt8(max(0, min(255,
                        (source.sample(sourceX + Double(x) - lane,
                            Double(box.top) + 5 + 7 * t, c) * edge).rounded())))
                }
            }
        }
        shin.over(result)
        return shin
    }
    return result
}

private func validateConnected(_ frame: Raster, pose: Int) throws {
    var visited = [Bool](repeating: false, count: cellWidth * cellHeight), areas: [Int] = []
    for start in visited.indices where !visited[start] && frame.rgba[start * 4 + 3] >= 32 {
        var queue = [start], cursor = 0; visited[start] = true
        while cursor < queue.count {
            let p = queue[cursor]; cursor += 1
            let x = p % cellWidth, y = p / cellWidth
            for dy in -1...1 { for dx in -1...1 {
                let nx = x + dx, ny = y + dy
                guard nx >= 0, nx < cellWidth, ny >= 0, ny < cellHeight else { continue }
                let next = ny * cellWidth + nx
                if !visited[next] && frame.rgba[next * 4 + 3] >= 32 {
                    visited[next] = true; queue.append(next)
                }
            } }
        }
        if queue.count >= 24 { areas.append(queue.count) }
    }
    guard areas.count == 1 else { throw BakeError.failed("Detached limb at pose \(pose + 1): components \(areas)") }
}

private func groundedTrot(_ master: Raster, standing: Raster, tracks: [[HoofTrack]], directory: String) throws -> ([[Raster]], String) {
    let layers = try hoofLayers(master)
    let standingLayers = try hoofLayers(standing)
    let bodies = (0..<coats).map { coat in
        // Recolour before removing standing hooves, matching slot 0 exactly.
        var standingBody = recolor(standing, coat)
        for i in stride(from: 0, to: standingBody.rgba.count, by: 4) {
            if standingLayers.body.rgba[i + 3] == 0 {
                for c in 0..<4 { standingBody.rgba[i + c] = 0 }
            }
        }
        return anchoredStandingHead(recolor(layers.body, coat, completeRunFleece: true),
                                    standingBody: standingBody)
    }
    try FileManager.default.createDirectory(atPath: directory, withIntermediateDirectories: true)
    try write(layers.body, directory + "/neutral-body.png")
    var frames = [[Raster]](repeating: [], count: coats), visible = [Int](repeating: 0, count: 4)
    var visibleStance = [Int](repeating: 0, count: 4)
    for pose in 0..<runCount {
        let phase = Double(pose) / Double(runCount)
        let hooves = (0..<4).map { placedHoof(layers.hooves[$0], track: tracks[pose][$0], leg: $0) }
        var footLayer = Raster(cellWidth, cellHeight)
        for hoof in hooves { footLayer.over(hoof) }
        let whiteBody = weightShiftedBody(bodies[0], phase: phase)
        for leg in 0..<4 {
            var area = 0.0
            for i in stride(from: 0, to: whiteBody.rgba.count, by: 4) {
                area += Double(hooves[leg].rgba[i + 3]) / 255 * (1 - Double(whiteBody.rgba[i + 3]) / 255)
            }
            if area >= 8 {
                visible[leg] += 1
                if tracks[pose][leg].stance { visibleStance[leg] += 1 }
            }
        }
        for coat in 0..<coats {
            var frame = footLayer
            frame.over(coat == 0 ? whiteBody : weightShiftedBody(bodies[coat], phase: phase))
            if coat == 0 { try validateConnected(frame, pose: pose) }
            frames[coat].append(frame)
        }
    }
    guard visible.allSatisfy({ $0 >= 12 }) else { throw BakeError.failed("Hooves hidden throughout gait: \(visible)") }
    guard visibleStance.allSatisfy({ $0 >= 3 }) else { throw BakeError.failed("No readable contact poses: \(visibleStance)") }
    let report = "Contact-locked trot: 96 poses; 4 fixed-X hoof lanes; diagonal pairs; C++ ground-contact tracks; visible frames per hoof \(visible), visible planted frames \(visibleStance). Wool compression/weight shift and tail lag are independent of hooves."
    print(report)
    return (frames, report)
}

private func validate(_ frames: [Raster], original: Raster) throws {
    var all = Bounds(left: cellWidth, top: cellHeight, right: 0, bottom: 0)
    var maxSide = 0.0, maxForward = 0.0
    let whiteStanding = original.crop(0, 0, 512, 1024).resized(cellWidth, cellHeight)
    for coat in 0..<coats {
        let standing = recolor(whiteStanding, coat)
        guard frames[coat * posesPerCoat].rgba == standing.rgba else {
            throw BakeError.failed("Standing sprite/coat pipeline changed for coat \(coat)")
        }
        for pose in 0..<posesPerCoat {
            let frame = frames[coat * posesPerCoat + pose], box = bounds(frame)
            if pose > 0 {
                let firstRun = frames[coat * posesPerCoat + 1]
                guard abs(frontCenter(frame) - frontCenter(firstRun)) <= 1.5,
                    abs(box.top - bounds(firstRun).top) <= 1 else {
                    throw BakeError.failed("Head registration drift, coat \(coat), pose \(pose)")
                }
                try validateHeadJoin(frame, standing: standing, coat: coat, pose: pose)
            }
            guard box.left >= 24 && box.right < cellWidth - 24 && box.top >= 12 && box.bottom < cellHeight - 12 else {
                throw BakeError.failed("Insufficient atlas padding coat \(coat), pose \(pose): \(box)")
            }
            all.left = min(all.left, box.left); all.right = max(all.right, box.right)
            all.top = min(all.top, box.top); all.bottom = max(all.bottom, box.bottom)
            for y in 0..<cellHeight { for x in 0..<cellWidth {
                let i = frame.index(x, y)
                // Keep every visible pixel inside the independent gameplay
                // traffic/fence envelope (.48 x .92 half-extents in cells).
                if frame.rgba[i + 3] >= 16 {
                    let side = abs((Double(x) + 0.5) / Double(cellWidth) - 0.5) * 1.23
                    let forward = abs((Double(y) + 0.5) / Double(cellHeight) - 0.5) * 1.97
                    maxSide = max(maxSide, side); maxForward = max(maxForward, forward)
                }
                if coat > 0 && frame.rgba[i + 3] != frames[pose].rgba[i + 3] {
                    throw BakeError.failed("Coat silhouette changed between frames")
                }
                // Preserve common face colours where the standing variants
                // agree. Their neck/ear antialias fringe can legitimately
                // include coat-coloured wool; do not force it back to white.
                // Exact stand/run head identity is checked above for ALL coats.
                let white = frames[pose], alpha = Double(white.rgba[i + 3]) / 255
                let red = alpha > 0 ? Double(white.rgba[i]) / alpha : 0
                let green = alpha > 0 ? Double(white.rgba[i + 1]) / alpha : 0
                let blue = alpha > 0 ? Double(white.rgba[i + 2]) / alpha : 0
                let atEar = abs(Double(x) - Double(cellWidth - 1) * 0.5) > Double(cellWidth) * 0.15
                let pinkEar = atEar && y < box.top + Int(Double(box.height) * 0.28)
                    && red - green > 35 && green - blue < 45 && red - blue > 50
                let commonStandingFace = (0..<4).allSatisfy { standing.rgba[i + $0] == whiteStanding.rgba[i + $0] }
                if coat > 0 && pose > 0 && commonStandingFace &&
                    (y < box.top + Int(Double(box.height) * 0.15) || pinkEar) {
                    for channel in 0..<3 where frame.rgba[i + channel] != frames[pose].rgba[i + channel] {
                        throw BakeError.failed("Recolouring changed muzzle/eyes: coat \(coat), pose \(pose), x/y \(x)/\(y), white RGBA \(Array(frames[pose].rgba[i..<i + 4]))")
                    }
                }
                if x < 8 || x >= cellWidth - 8 || y < 8 || y >= cellHeight - 8 {
                    guard frame.rgba[i + 3] == 0 else { throw BakeError.failed("Nontransparent cell border") }
                }
                for channel in 0..<3 where frame.rgba[i + channel] > frame.rgba[i + 3] {
                    throw BakeError.failed("Invalid premultiplied alpha")
                }
                if coat > 0 && x > 92 && x < 164 && y > 135 && y < 425 {
                    let a = Double(frame.rgba[i + 3]) / 255
                    if a > 0.5 {
                        let smallest = (0..<3).map { Double(frame.rgba[i + $0]) / a }.min()!
                        guard smallest < 155 else {
                            throw BakeError.failed("Bright white fleece island: coat \(coat), pose \(pose), pixel \(x),\(y), RGBA \(Array(frame.rgba[i..<i + 4]))")
                        }
                    }
                }
            } }
        }
        for a in 1...runCount { for b in (a + 1)..<posesPerCoat {
            guard frames[coat * posesPerCoat + a].rgba != frames[coat * posesPerCoat + b].rgba else {
                throw BakeError.failed("Duplicate run frames \(a)/\(b)")
            }
        } }
        if coat == 0 { print("White-coat all-pose visible half-extents: \(maxSide) x \(maxForward) cells") }
    }
    print("Measured visible half-extents: \(maxSide) x \(maxForward) cells")
    guard maxSide <= 0.48 && maxForward <= 0.92 else {
        throw BakeError.failed("Visible pixels exceed traffic envelope: \(maxSide) x \(maxForward) cells")
    }
    print("Validated: \(runCount) unique whole-animal poses × \(coats) coats; standing/run heads pixel-identical; opaque neck joins; standing unchanged; alpha/padding/coat checks pass")
    print("All-pose visible bounds: \(all)")
}

private struct ProgressionMetrics {
    var meanContourChange = 0.0, maxContourChange = 0.0
    var maxBoundaryStep = 0.0, maxCentroidStep = 0.0, maxColorStep = 0.0
}

private func progression(_ frames: [Raster]) -> ProgressionMetrics {
    let mattes = frames.map { DistanceMatte($0) }
    func centroid(_ frame: Raster) -> SIMD2<Double> {
        var weighted = SIMD2<Double>.zero, area = 0.0
        for y in 0..<frame.height { for x in 0..<frame.width {
            let alpha = Double(frame.rgba[frame.index(x, y) + 3]) / 255
            weighted += SIMD2(Double(x), Double(y)) * alpha; area += alpha
        } }
        return weighted / max(1, area)
    }
    let centroids = frames.map { centroid($0) }
    var metrics = ProgressionMetrics()
    for i in 0..<frames.count {
        let next = (i + 1) % frames.count, first = frames[i], second = frames[next]
        var changed = 0.0, union = 0.0, colorChange = 0.0
        for p in 0..<(first.width * first.height) {
            let a = first.rgba[p * 4 + 3] >= 128, b = second.rgba[p * 4 + 3] >= 128
            if a || b { union += 1 }
            if a != b { changed += 1 }
            if abs(mattes[i].distance[p]) <= 0.5 {
                metrics.maxBoundaryStep = max(metrics.maxBoundaryStep, Double(abs(mattes[next].distance[p])))
            }
            if abs(mattes[next].distance[p]) <= 0.5 {
                metrics.maxBoundaryStep = max(metrics.maxBoundaryStep, Double(abs(mattes[i].distance[p])))
            }
            for c in 0..<3 { colorChange += abs(Double(first.rgba[p * 4 + c]) - Double(second.rgba[p * 4 + c])) }
        }
        let ratio = changed / max(1, union), delta = centroids[next] - centroids[i]
        metrics.meanContourChange += ratio / Double(frames.count)
        metrics.maxContourChange = max(metrics.maxContourChange, ratio)
        metrics.maxCentroidStep = max(metrics.maxCentroidStep, sqrt(delta.x * delta.x + delta.y * delta.y))
        metrics.maxColorStep = max(metrics.maxColorStep, colorChange / max(1, union) / 3)
    }
    return metrics
}

private func validateProgression(_ keys: [Raster], run: [Raster]) throws -> String {
    let original = progression(keys), smooth = progression(run)
    let report = """
    Cyclic frame progression (including final -> first):
      8 keys: contour mean/max \(original.meanContourChange)/\(original.maxContourChange), boundary max \(original.maxBoundaryStep) px, centroid max \(original.maxCentroidStep) px, RGB step max \(original.maxColorStep)
      \(run.count) poses: contour mean/max \(smooth.meanContourChange)/\(smooth.maxContourChange), boundary max \(smooth.maxBoundaryStep) px, centroid max \(smooth.maxCentroidStep) px, RGB step max \(smooth.maxColorStep)
    """
    print(report)
    guard smooth.meanContourChange < original.meanContourChange * 0.65,
        smooth.maxContourChange < original.maxContourChange * 0.85,
        smooth.maxBoundaryStep < original.maxBoundaryStep else {
        throw BakeError.failed("In-betweens do not sufficiently reduce silhouette discontinuity")
    }
    return report
}

private func preview(_ frames: [Raster], directory: String, metrics: String) throws {
    try FileManager.default.createDirectory(atPath: directory, withIntermediateDirectories: true)
    var contact = Raster(cellWidth * columns, cellHeight * rows)
    for coat in 0..<coats { for pose in 0..<posesPerCoat {
        let slot = coat * slotsPerCoat + pose
        contact.insert(frames[coat * posesPerCoat + pose], (slot % columns) * cellWidth, (slot / columns) * cellHeight)
    } }
    try write(contact, directory + "/game-sheep-contact.png")
    // Readable close-ups of the previously popping midpoint and cycle seam.
    for (name, poses) in [("start-stop", [0, 1, 25, 49, 73, 96]),
                           ("stride", [1, 17, 33, 49, 65, 81]),
                           ("midpoint", [4, 5, 6, 7, 8, 9]),
                           ("seam", [runCount - 2, runCount - 1, runCount, 1, 2, 3])] {
        var detail = Raster(cellWidth * poses.count, cellHeight * coats)
        for coat in 0..<coats { for (column, pose) in poses.enumerated() {
            detail.insert(frames[coat * posesPerCoat + pose], column * cellWidth, coat * cellHeight)
        } }
        try write(detail, directory + "/game-sheep-\(name).png")
    }
    // No GIF: its common >= 20ms frame clamp would show the wrong cadence.
    // requestAnimationFrame samples elapsed phase, like the runtime's
    // distance-based gait, and naturally skips atlas poses on 60Hz displays.
    let html = """
    <!doctype html><html lang="en"><meta charset="utf-8"><title>Gameplay sheep — \(runCount) poses</title>
    <style>body{margin:24px;background:#253322;color:#fff;font:16px system-ui}canvas{background:#6c9142;max-width:100%;height:auto}pre{white-space:pre-wrap}</style>
    <h1>\(runCount)-pose grounded trot</h1><p>Full speed: 5.5 cells/s; 1.6 cells/stride; \(previewCycleSeconds) s/cycle. Ground markers move at actual travel speed, as if the camera follows the sheep. Compare foot contact against the markers, not only an animal running in place.</p>
    <h2>Runtime cadence — 1×</h2><canvas id="normal" width="960" height="512"></canvas>
    <h2>Inspection — ¼ speed</h2><canvas id="slow" width="960" height="512"></canvas>
    <h2>Standing → running → standing (head colour / neck join)</h2><canvas id="startStop" width="960" height="512"></canvas>
    <p id="frame"></p><pre>\(metrics)</pre><script>
    const atlas=new Image();atlas.src='game-sheep-contact.png';
    const cycle=\(previewCycleSeconds)*1000;
    const columns=\(columns),slotsPerCoat=\(slotsPerCoat),runCount=\(runCount);
    const normal=document.querySelector('#normal').getContext('2d'),slow=document.querySelector('#slow').getContext('2d'),startStop=document.querySelector('#startStop').getContext('2d');
    // Match the runtime quad's 1.23:1.97 aspect, not the padded PNG cell's 1:2.
    const drawWidth=512*1.23/1.97;
    function paint(ctx,pose,elapsed){ctx.clearRect(0,0,960,512);const step=512/1.97/4,scroll=(elapsed/1000*5.5*512/1.97)%step;ctx.strokeStyle='#93ae70';ctx.lineWidth=1;for(let y=scroll-step;y<512;y+=step){ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(960,y);ctx.stroke();}for(let coat=0;coat<3;coat++){const slot=coat*slotsPerCoat+pose;ctx.drawImage(atlas,(slot%columns)*256,Math.floor(slot/columns)*512,256,512,coat*320+(320-drawWidth)/2,0,drawWidth,512);}}
    atlas.onload=()=>{const start=performance.now();function frame(now){const elapsed=now-start;const pose=1+Math.floor((elapsed%cycle)/cycle*runCount),slowPose=1+Math.floor(((elapsed/4)%cycle)/cycle*runCount);paint(normal,pose,elapsed);paint(slow,slowPose,elapsed/4);const moving=elapsed%2400>=800;paint(startStop,moving?pose:0,moving?elapsed:0);document.querySelector('#frame').textContent=`Full speed pose ${pose}/${runCount}; slow pose ${slowPose}/${runCount}`;requestAnimationFrame(frame);}requestAnimationFrame(frame);};
    </script></html>
    """
    try html.write(toFile: directory + "/game-sheep-run.html", atomically: true, encoding: .utf8)
    try metrics.write(toFile: directory + "/game-sheep-metrics.txt", atomically: true, encoding: .utf8)
}

private func compareHeadFix(_ previous: Raster, _ current: Raster) throws {
    guard previous.width == columns * cellWidth, previous.height == rows * cellHeight,
          current.width == previous.width, current.height == previous.height else {
        throw BakeError.failed("Head comparison expects two 25x12 atlases")
    }
    func cell(_ atlas: Raster, _ slot: Int) -> Raster {
        atlas.crop((slot % columns) * cellWidth, (slot / columns) * cellHeight, cellWidth, cellHeight)
    }
    let bodyBytes = (cellHeight - headJoinEnd) * cellWidth * 4
    var oldHeadMismatches = 0
    for coat in 0..<coats {
        let standing = cell(current, coat * slotsPerCoat)
        guard standing.rgba == cell(previous, coat * slotsPerCoat).rgba else {
            throw BakeError.failed("Head fix changed the standing sheep, coat \(coat)")
        }
        for pose in 1...runCount {
            let before = cell(previous, coat * slotsPerCoat + pose)
            let after = cell(current, coat * slotsPerCoat + pose)
            try validateHeadJoin(after, standing: standing, coat: coat, pose: pose)
            try validateConnected(after, pose: pose - 1)
            guard before.rgba.suffix(bodyBytes) == after.rgba.suffix(bodyBytes) else {
                throw BakeError.failed("Head fix altered running shoulders/body/feet below row \(headJoinEnd), coat \(coat), pose \(pose)")
            }
            for i in 0..<(cellWidth * fixedHeadRows * 4) where before.rgba[i] != standing.rgba[i] {
                oldHeadMismatches += 1
            }
        }
        for pose in posesPerCoat..<slotsPerCoat {
            guard cell(current, coat * slotsPerCoat + pose).rgba.allSatisfy({ $0 == 0 }) else {
                throw BakeError.failed("Head fix touched empty atlas cells")
            }
        }
    }
    print("Head-fix regression: \(oldHeadMismatches) old standing/run head byte mismatches -> 0; all 288 running heads match their standing coat; neck core alpha >=240; connected silhouettes; standing, rows \(headJoinEnd)...511 and empty cells preserved")
}

private func compareAnimation(_ previous: Raster, _ current: Raster) throws {
    let oldColumns = 25, oldSlots = 100, oldRunCount = 96
    guard previous.width == oldColumns * cellWidth && previous.height == 12 * cellHeight,
        current.width == columns * cellWidth && current.height == rows * cellHeight,
        runCount == oldRunCount else {
        throw BakeError.failed("Animation comparison expects two 96-pose 25x12 atlases")
    }
    func cell(_ atlas: Raster, _ slot: Int) -> Raster {
        let width = atlas.width / cellWidth
        return atlas.crop((slot % width) * cellWidth, (slot / width) * cellHeight, cellWidth, cellHeight)
    }
    for coat in 0..<coats {
        guard cell(previous, coat * oldSlots).rgba == cell(current, coat * slotsPerCoat).rgba else {
            throw BakeError.failed("Animation update changed standing pose: coat \(coat)")
        }
    }
    let oldFrames = (1...oldRunCount).map { cell(previous, $0) }
    let newFrames = (1...runCount).map { cell(current, $0) }
    let old = progression(oldFrames), new = progression(newFrames)
    guard new.meanContourChange < old.meanContourChange,
        new.maxContourChange < old.maxContourChange,
        new.maxCentroidStep < old.maxCentroidStep,
        new.maxColorStep < old.maxColorStep else {
        throw BakeError.failed("New animation did not improve closed-cycle colour/contour progression")
    }
    print("Standing poses pixel-identical for all coats")
    print("Old sway -> grounded trot (both 96): contour mean \(old.meanContourChange) -> \(new.meanContourChange), max \(old.maxContourChange) -> \(new.maxContourChange); boundary max \(old.maxBoundaryStep) -> \(new.maxBoundaryStep) px; centroid max \(old.maxCentroidStep) -> \(new.maxCentroidStep) px; RGB max \(old.maxColorStep) -> \(new.maxColorStep)")
    // Compare at equal display cadence too, not just adjacent atlas cells.
    // More source frames do NOT mean every one is shown on a 60 Hz display.
    func sampledRear(_ frames: [Raster], fps: Int) -> (rgb: Double, acceleration: Double) {
        var rgbMax = 0.0, acceleration = 0.0, centers: [Double] = []
        for tick in 0...(fps * 2) {
            let phase = Double(tick) / Double(fps) / previewCycleSeconds
            let frame = frames[Int(floor(phase * Double(frames.count))) % frames.count]
            centers.append(rearCenter(frame))
            if tick > 0 {
                let lastPhase = Double(tick - 1) / Double(fps) / previewCycleSeconds
                let previous = frames[Int(floor(lastPhase * Double(frames.count))) % frames.count]
                var rgb = 0.0, area = 0.0
                for y in (cellHeight * 2 / 3)..<cellHeight { for x in 0..<cellWidth {
                    let i = frame.index(x, y)
                    if frame.rgba[i + 3] > 0 || previous.rgba[i + 3] > 0 { area += 1 }
                    for c in 0..<3 { rgb += abs(Double(frame.rgba[i + c]) - Double(previous.rgba[i + c])) }
                } }
                rgbMax = max(rgbMax, rgb / max(1, area) / 3)
            }
            if tick > 1 { acceleration = max(acceleration, abs(centers[tick] - 2 * centers[tick - 1] + centers[tick - 2])) }
        }
        return (rgbMax, acceleration)
    }
    for fps in [30, 60, 120] {
        let before = sampledRear(oldFrames, fps: fps), after = sampledRear(newFrames, fps: fps)
        print("Rear at \(fps) FPS: RGB step max \(before.rgb) -> \(after.rgb); centroid second difference \(before.acceleration) -> \(after.acceleration) px")
    }
}

private func main() throws {
    let args = CommandLine.arguments
    if args.count == 2 && args[1] == "--self-test" { try selfTest(); return }
    if args.count == 4 && args[1] == "--compare-head-fix" {
        try compareHeadFix(read(args[2]), read(args[3])); return
    }
    if args.count == 4 && args[1] == "--compare-animation" {
        try compareAnimation(read(args[2]), read(args[3])); return
    }
    if args.count == 4 && args[1] == "--compare-motion" {
        let old = try read(args[2]), new = try read(args[3])
        guard old.width == 2560 && old.height == 1536,
            new.width == columns * cellWidth && new.height == rows * cellHeight else {
            throw BakeError.failed("Motion comparison expects old 10x3 and new \(columns)x\(rows) atlases")
        }
        func excursion(_ atlas: Raster, slots: [Int], lower: Bool) -> Double {
            let atlasColumns = atlas.width / cellWidth
            let values = slots.map { slot -> Double in
                let frame = atlas.crop((slot % atlasColumns) * cellWidth, (slot / atlasColumns) * cellHeight, cellWidth, cellHeight)
                var area = 0.0, weightedX = 0.0
                let start = lower ? cellHeight * 2 / 3 : 0, end = lower ? cellHeight : cellHeight / 3
                for y in start..<end { for x in 0..<cellWidth {
                    let alpha = Double(frame.rgba[frame.index(x, y) + 3]) / 255
                    area += alpha; weightedX += Double(x) * alpha
                } }
                return weightedX / max(1, area)
            }
            return values.max()! - values.min()!
        }
        for lower in [false, true] {
            print("\(lower ? "Rear third" : "Front third") alpha-centroid lateral excursion: old \(excursion(old, slots: Array(1...keyCount), lower: lower)) px; new \(excursion(new, slots: (0..<keyCount).map { 1 + samplesPerKey * $0 }, lower: lower)) px")
        }
        return
    }
    guard args.count == 6 else {
        throw BakeError.failed("usage: build_game_sheep_run.swift SOURCE_4X2.png EXISTING_TOPDOWN_3X1.png OUTPUT.png PREVIEW_DIR TROT_TRACKS.csv")
    }
    let source = try read(args[1]), original = try read(args[2])
    guard source.width == source.height, original.width == 1536, original.height == 1024 else {
        throw BakeError.failed("Unexpected source layout")
    }
    var clean: [Raster] = []
    for frame in 0..<8 {
        let left = Int((Double(frame % 4) * Double(source.width) / 4).rounded())
        let right = Int((Double(frame % 4 + 1) * Double(source.width) / 4).rounded())
        let top = frame / 4 * (source.height / 2)
        clean.append(try matte(source.crop(left, top, right - left, source.height / 2)))
    }
    let standing = original.crop(0, 0, 512, 1024).resized(cellWidth, cellHeight)
    let target = bounds(standing), master = bounds(clean[0])
    let masterFront = frontCenter(clean[0])
    var run: [Raster] = []
    for frame in 0..<8 {
        // Identical scale for all frames; nose-height and head registration
        // stay fixed while the painted pelvis and hooves move inside the cell.
        let box = bounds(clean[frame])
        let aligned = Bounds(left: master.left + Int((frontCenter(clean[frame]) - masterFront).rounded()),
            top: box.top, right: 0, bottom: 0)
        let sampleBox = Bounds(left: aligned.left, top: aligned.top,
            right: aligned.left + master.width - 1, bottom: aligned.top + master.height - 1)
        run.append(fitted(clean[frame], sourceBox: sampleBox, target: target))
    }
    let tracks = try readTracks(args[5])
    let (smooth, contactMetrics) = try groundedTrot(run[0], standing: standing, tracks: tracks, directory: args[4])
    let metrics = try validateProgression(run, run: smooth[0]) + "\n" + contactMetrics
    guard columns * rows == coats * slotsPerCoat else { throw BakeError.failed("Invalid atlas layout") }
    var atlas = Raster(columns * cellWidth, rows * cellHeight), frames: [Raster] = []
    for coat in 0..<coats {
        // Coats were derived on the undeformed neutral drawing. The same
        // transforms, hoof layers and occlusion apply to all three coats.
        for pose in 0..<posesPerCoat {
            let painted = pose == 0 ? recolor(standing, coat) : smooth[coat][pose - 1]
            let slot = coat * slotsPerCoat + pose
            atlas.insert(painted, (slot % columns) * cellWidth, (slot / columns) * cellHeight)
            frames.append(painted)
        }
    }
    try validate(frames, original: original)
    for coat in 0..<coats { for padding in posesPerCoat..<slotsPerCoat {
        let slot = coat * slotsPerCoat + padding
        guard atlas.crop((slot % columns) * cellWidth, (slot / columns) * cellHeight,
            cellWidth, cellHeight).rgba.allSatisfy({ $0 == 0 }) else { throw BakeError.failed("Opaque atlas padding") }
    } }
    try write(atlas, args[3]); try preview(frames, directory: args[4], metrics: metrics)
    print("Wrote \(args[3]): \(atlas.width)x\(atlas.height), \(columns)x\(rows), four rows/coat; standing 0 + run 1...\(runCount) + empty \(posesPerCoat)...\(slotsPerCoat - 1)")
    print("Standing alpha bounds \(target); run master \(master); preview \(args[4])")
}
do { try main() } catch { fputs("\(error)\n", stderr); exit(1) }
