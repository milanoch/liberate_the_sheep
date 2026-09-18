#!/usr/bin/env swift

// Offline import/atlas packing of whole-animal imagegen poses, never a limb rig.
// Existing white standing pose is copied, not redrawn. Standing and running
// share one coat treatment so clicking never changes fleece/ear colour.
// Eleven motion-compensated in-betweens on EVERY edge, including key 8 -> 1.
// Continuous warped colour and a smooth rear-only sway, no runtime deformation.
// Compile with swiftc -O; macOS 13+ Vision optical-flow revision 2 is required.
import CoreGraphics
import CoreVideo
import Foundation
import ImageIO
import UniformTypeIdentifiers
import Vision

private let cellWidth = 256, cellHeight = 512, columns = 25, rows = 12, coats = 3
private let keyCount = 8, samplesPerKey = 12
private let runCount = keyCount * samplesPerKey
private let posesPerCoat = runCount + 1, slotsPerCoat = 100
private let previewCycleSeconds = 1.6 / 5.5
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
    func opaqueForFlow() -> Raster {
        var result = self
        for i in stride(from: 0, to: rgba.count, by: 4) {
            let remaining = 1 - Double(rgba[i + 3]) / 255
            for c in 0..<3 {
                result.rgba[i + c] = UInt8(min(255,
                    Double(rgba[i + c]) + [122.0, 156.0, 168.0][c] * remaining))
            }
            result.rgba[i + 3] = 255
        }
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

private func rearWeight(_ y: Int) -> Double {
    let t = max(0, min(1, (Double(y) / Double(cellHeight) - 0.625) / 0.195))
    return t * t * (3 - 2 * t)
}

private func smoothRearMotion(_ frames: inout [[Raster]], keys: [Raster]) throws -> String {
    // Normalize the rear's existing uneven lateral movement onto ONE smooth
    // periodic curve, rather than amplifying jumps independently on eight keys.
    // The fitted phase preserves the relationship to the painted hoof contacts.
    let centers = keys.map { rearCenter($0) }, tau = 2 * Double.pi
    let paintedCenter = centers.reduce(0, +) / Double(keyCount)
    var sine = 0.0, cosine = 0.0
    for key in 0..<keyCount {
        let phase = tau * Double(key) / Double(keyCount)
        sine += (centers[key] - paintedCenter) * sin(phase)
        cosine += (centers[key] - paintedCenter) * cos(phase)
    }
    let origin = atan2(cosine, sine)
    // Fit ONE amplitude/centre for the entire cycle inside the existing traffic
    // envelope. Never clamp individual poses: that would flatten peaks and jerk.
    var responses: [Double] = [], safeCenters: [ClosedRange<Double>] = []
    for frame in frames[0] {
        var mass = 0.0, weighted = 0.0
        var minShift = -Double.infinity, maxShift = Double.infinity
        for y in 0..<cellHeight { for x in 0..<cellWidth {
            let alpha = Double(frame.rgba[frame.index(x, y) + 3]) / 255, weight = rearWeight(y)
            if y >= cellHeight * 2 / 3 { mass += alpha; weighted += alpha * weight }
            if alpha >= 16.0 / 255 && weight > 0 {
                // One-pixel sampling margin inside .48-cell half width.
                minShift = max(minShift, (29 - Double(x)) / weight)
                maxShift = min(maxShift, (226 - Double(x)) / weight)
            }
        } }
        let response = weighted / max(1, mass), current = rearCenter(frame)
        responses.append(response)
        guard minShift <= maxShift else { throw BakeError.failed("No safe rear translation") }
        safeCenters.append((current + minShift * response)...(current + maxShift * response))
    }
    var amplitude = 14.0, center = paintedCenter, fitted = false, lastLow = 0.0, lastHigh = 0.0
    while amplitude >= 10 {
        var low = paintedCenter - 6, high = paintedCenter + 6
        for pose in 0..<runCount {
            let offset = amplitude * sin(tau * Double(pose) / Double(runCount) + origin)
            low = max(low, safeCenters[pose].lowerBound - offset)
            high = min(high, safeCenters[pose].upperBound - offset)
        }
        lastLow = low; lastHigh = high
        if low <= high { center = max(low, min(high, paintedCenter)); fitted = true; break }
        amplitude -= 0.25
    }
    guard fitted else { throw BakeError.failed("Smooth rear cycle cannot fit traffic envelope: painted center \(paintedCenter), range \(lastLow)...\(lastHigh)") }
    var measured: [Double] = [], worstError = 0.0
    for pose in 0..<runCount {
        let white = frames[0][pose]
        let target = center + amplitude * sin(tau * Double(pose) / Double(runCount) + origin)
        let shift = (target - rearCenter(white)) / responses[pose]
        for coat in 0..<coats {
            let source = frames[coat][pose]
            var output = source
            for y in (cellHeight / 2)..<cellHeight {
                let offset = shift * rearWeight(y)
                for x in 0..<cellWidth { for c in 0..<4 {
                    output.rgba[output.index(x, y) + c] = UInt8(max(0, min(255,
                        source.sample(Double(x) - offset, Double(y), c).rounded())))
                } }
            }
            guard output.rgba.prefix(cellWidth * cellHeight / 2 * 4) ==
                source.rgba.prefix(cellWidth * cellHeight / 2 * 4) else {
                throw BakeError.failed("Rear sway moved the front half")
            }
            frames[coat][pose] = output
        }
        measured.append(rearCenter(frames[0][pose]))
        worstError = max(worstError, abs(measured.last! - target))
    }
    var maxStep = 0.0, maxAcceleration = 0.0
    for pose in 0..<runCount {
        let previous = measured[(pose + runCount - 1) % runCount]
        let current = measured[pose], next = measured[(pose + 1) % runCount]
        maxStep = max(maxStep, abs(next - current))
        maxAcceleration = max(maxAcceleration, abs(next - 2 * current + previous))
    }
    guard worstError < 0.08, maxStep < 1.05, maxAcceleration < 0.12 else {
        throw BakeError.failed("Rear cycle not smooth: error \(worstError), step \(maxStep), acceleration \(maxAcceleration)")
    }
    let report = "Rear-only periodic sway: amplitude \(amplitude), center \(center), excursion \(measured.max()! - measured.min()!) px; max step \(maxStep), acceleration \(maxAcceleration), target error \(worstError) px (including seam); front half unchanged"
    print(report)
    return report
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

private func retainFaceColors(_ colored: Raster, white: Raster) -> Raster {
    // Bilinear sampling at the ear/fleece boundary can mix a coloured fleece
    // neighbour into an otherwise unchanged ear pixel. Face/ears have no coat
    // variant: use the identically warped white master's RGB for this region.
    // Do not touch alpha, wool highlights, or the body/limb animation.
    var output = colored
    let box = bounds(white), muzzleBottom = box.top + Int(Double(box.height) * 0.16)
    let earBottom = box.top + Int(Double(box.height) * 0.28)
    for y in 0..<earBottom { for x in 0..<cellWidth {
        let i = white.index(x, y), alpha = Double(white.rgba[i + 3]) / 255
        guard alpha > 0 else { continue }
        let red = Double(white.rgba[i]) / alpha, green = Double(white.rgba[i + 1]) / alpha
        let blue = Double(white.rgba[i + 2]) / alpha
        let atEar = abs(Double(x) - Double(cellWidth - 1) * 0.5) > Double(cellWidth) * 0.15
        let pinkEar = atEar && red - green > 35 && green - blue < 45 && red - blue > 50
        if y < muzzleBottom || pinkEar {
            for c in 0..<3 { output.rgba[i + c] = white.rgba[i + c] }
        }
    } }
    return output
}

// The bidirectional Vision/SDF method is shared conceptually with the existing
// build_sheep_inbetweens.swift importer. Kept self-contained so this gameplay
// baker does not execute or modify the menu importer as a side effect.
private struct Flow {
    let width: Int, height: Int
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

private func opticalFlow(_ first: Raster, _ second: Raster) throws -> Flow {
    try autoreleasepool {
        let request = VNGenerateOpticalFlowRequest(targetedCGImage: try second.opaqueForFlow().image(), options: [:])
        request.revision = VNGenerateOpticalFlowRequestRevision2
        request.computationAccuracy = .veryHigh
        request.outputPixelFormat = kCVPixelFormatType_TwoComponent32Float
        try VNImageRequestHandler(cgImage: first.opaqueForFlow().image(), options: [:]).perform([request])
        guard let buffer = request.results?.first?.pixelBuffer else { throw BakeError.failed("Vision returned no flow") }
        CVPixelBufferLockBaseAddress(buffer, .readOnly)
        defer { CVPixelBufferUnlockBaseAddress(buffer, .readOnly) }
        guard let base = CVPixelBufferGetBaseAddress(buffer) else { throw BakeError.failed("Flow inaccessible") }
        let width = CVPixelBufferGetWidth(buffer), height = CVPixelBufferGetHeight(buffer)
        guard width == first.width && height == first.height else { throw BakeError.failed("Unexpected flow dimensions") }
        let rowBytes = CVPixelBufferGetBytesPerRow(buffer)
        var vectors = [SIMD2<Float>](repeating: .zero, count: width * height)
        for y in 0..<height {
            let row = base.advanced(by: y * rowBytes).assumingMemoryBound(to: Float.self)
            for x in 0..<width {
                let vector = SIMD2(row[x * 2], row[x * 2 + 1])
                guard vector.x.isFinite && vector.y.isFinite else { throw BakeError.failed("Non-finite flow") }
                vectors[y * width + x] = vector
            }
        }
        return Flow(width: width, height: height, vectors: vectors)
    }
}

private func interpolate(_ first: Raster, _ second: Raster, forward: Flow, backward: Flow,
                        firstMatte: DistanceMatte, secondMatte: DistanceMatte, t: Float) -> Raster {
    if t <= 0 { return first }
    if t >= 1 { return second }
    var result = Raster(first.width, first.height)
    for y in 0..<first.height { for x in 0..<first.width {
        let target = SIMD2<Float>(Float(x), Float(y))
        var from = target, to = target
        for _ in 0..<6 {
            from = target - t * forward.sample(from.x, from.y)
            to = target - (1 - t) * backward.sample(to.x, to.y)
        }
        let distance = (1 - t) * firstMatte.sample(from.x, from.y) + t * secondMatte.sample(to.x, to.y)
        let firstAlpha = first.sample(Double(from.x), Double(from.y), 3)
        let secondAlpha = second.sample(Double(to.x), Double(to.y), 3)
        // Preserve the keys' soft coverage at interval endpoints. Pure SDF
        // reconstruction used to snap their antialias fringe on every key.
        let sdfWeight = Double(4 * t * (1 - t))
        let warpedCoverage = ((1 - Double(t)) * firstAlpha + Double(t) * secondAlpha) / 255
        let opacity = (1 - sdfWeight) * warpedCoverage
            + sdfWeight * Double(max(0, min(1, distance + 0.5)))
        // Both samples are already warped to the SAME intermediate pose.
        // Hard switching at t=.5 made fleece detail pop regardless of frame
        // count. Blend registered detail continuously, with alpha confidence
        // at disocclusions; keep one SDF silhouette, not two ghosted animals.
        let blend = Double(t * t * (3 - 2 * t))
        let firstWeight = (1 - blend) * firstAlpha, secondWeight = blend * secondAlpha
        let coverage = firstWeight + secondWeight
        if coverage > 0 && opacity > 0 {
            let alpha = UInt8((opacity * 255).rounded()), index = result.index(x, y)
            result.rgba[index + 3] = alpha
            for channel in 0..<3 {
                let value = ((1 - blend) * first.sample(Double(from.x), Double(from.y), channel)
                    + blend * second.sample(Double(to.x), Double(to.y), channel)) / coverage * Double(alpha)
                result.rgba[index + channel] = UInt8(max(0, min(Double(alpha), value.rounded())))
            }
        }
    } }
    return result
}

private func selfTest() throws {
    var first = Raster(192, 192), second = first
    for y in 48..<144 { for x in 48..<144 {
        for c in 0..<3 {
            let value = UInt8(35 + (x * 17 + y * 19 + c * 31) % 180)
            first.rgba[first.index(x, y) + c] = value
            second.rgba[second.index(x + 12, y + 5) + c] = value
        }
        first.rgba[first.index(x, y) + 3] = 255
        second.rgba[second.index(x + 12, y + 5) + 3] = 255
    } }
    let forward = try opticalFlow(first, second), backward = try opticalFlow(second, first)
    let actual = forward.sample(90, 90), reverse = backward.sample(102, 95)
    guard abs(actual.x - 12) < 2, abs(actual.y - 5) < 2,
        abs(reverse.x + 12) < 2, abs(reverse.y + 5) < 2 else {
        throw BakeError.failed("Flow orientation failed: forward \(actual), backward \(reverse)")
    }
    let middle = interpolate(first, second, forward: forward, backward: backward,
        firstMatte: DistanceMatte(first), secondMatte: DistanceMatte(second), t: 0.5)
    let box = bounds(middle)
    guard abs(box.left - 54) <= 2, abs(box.top - 50) <= 2 else {
        throw BakeError.failed("Inverse sampling does not produce the midpoint: \(box)")
    }
    // A midpoint colour switch and mismatched soft-alpha endpoints must fail
    // independently of the atlas frame count or the particular sheep art.
    var light = Raster(32, 32), dark = light
    for y in 8..<24 { for x in 8..<24 {
        let i = light.index(x, y), alpha: UInt8 = x == 8 || x == 23 ? 112 : 255
        light.rgba[i + 3] = alpha; dark.rgba[i + 3] = alpha
        for channel in 0..<3 {
            light.rgba[i + channel] = UInt8(Double(alpha) * 0.85)
            dark.rgba[i + channel] = UInt8(Double(alpha) * 0.2)
        }
    } }
    let zero = Flow(width: 32, height: 32, vectors: .init(repeating: .zero, count: 1024))
    let lightMatte = DistanceMatte(light), darkMatte = DistanceMatte(dark)
    func sample(_ t: Float) -> Raster {
        interpolate(light, dark, forward: zero, backward: zero,
            firstMatte: lightMatte, secondMatte: darkMatte, t: t)
    }
    for (a, b) in [(sample(0.49999), sample(0.50001)), (light, sample(0.00001)), (dark, sample(0.99999))] {
        guard zip(a.rgba, b.rgba).allSatisfy({ abs(Int($0) - Int($1)) <= 1 }) else {
            throw BakeError.failed("Colour/soft-alpha discontinuity at midpoint or key boundary")
        }
    }
    print("Flow direction, continuous midpoint colour and soft-alpha endpoint self-tests passed: forward \(actual), reverse \(reverse), midpoint \(box)")
}

private func validate(_ frames: [Raster], original: Raster, keys: [[Raster]]) throws {
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
                if (pose - 1) % samplesPerKey == 0 {
                    let frontBytes = cellWidth * cellHeight / 2 * 4
                    guard frame.rgba.prefix(frontBytes) == keys[coat][(pose - 1) / samplesPerKey].rgba.prefix(frontBytes) else {
                        throw BakeError.failed("Source key front changed: coat \(coat), pose \(pose)")
                    }
                }
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
                // The muzzle and eyes occupy the first 15% of the sheep;
                // pink ears extend farther down alongside coloured forehead
                // wool, so test their colour separately instead of guarding
                // an entire horizontal band of legitimate fleece pixels.
                let white = frames[pose], alpha = Double(white.rgba[i + 3]) / 255
                let red = alpha > 0 ? Double(white.rgba[i]) / alpha : 0
                let green = alpha > 0 ? Double(white.rgba[i + 1]) / alpha : 0
                let blue = alpha > 0 ? Double(white.rgba[i + 2]) / alpha : 0
                let atEar = abs(Double(x) - Double(cellWidth - 1) * 0.5) > Double(cellWidth) * 0.15
                let pinkEar = atEar && y < box.top + Int(Double(box.height) * 0.28)
                    && red - green > 35 && green - blue < 45 && red - blue > 50
                if coat > 0 && pose > 0 && (y < box.top + Int(Double(box.height) * 0.15) || pinkEar) {
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
    print("Validated: \(runCount) unique whole-animal poses × \(coats) coats; \(keyCount) key fronts per coat exact; standing unchanged; alpha/padding/coat checks pass")
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
    for (name, poses) in [("midpoint", [4, 5, 6, 7, 8, 9]),
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
    <h1>\(runCount)-pose gameplay gait</h1><p>Full speed: 5.5 cells/s; 1.6 cells/stride; \(previewCycleSeconds) s/cycle. Compare identical motion at ¼ speed below. No travel or whole-body transform is applied in this pose preview.</p>
    <h2>Runtime cadence — 1×</h2><canvas id="normal" width="960" height="512"></canvas>
    <h2>Inspection — ¼ speed</h2><canvas id="slow" width="960" height="512"></canvas>
    <p id="frame"></p><pre>\(metrics)</pre><script>
    const atlas=new Image();atlas.src='game-sheep-contact.png';
    const cycle=\(previewCycleSeconds)*1000;
    const columns=\(columns),slotsPerCoat=\(slotsPerCoat),runCount=\(runCount);
    const normal=document.querySelector('#normal').getContext('2d'),slow=document.querySelector('#slow').getContext('2d');
    // Match the runtime quad's 1.23:1.97 aspect, not the padded PNG cell's 1:2.
    const drawWidth=512*1.23/1.97;
    function paint(ctx,pose){ctx.clearRect(0,0,960,512);for(let coat=0;coat<3;coat++){const slot=coat*slotsPerCoat+pose;ctx.drawImage(atlas,(slot%columns)*256,Math.floor(slot/columns)*512,256,512,coat*320+(320-drawWidth)/2,0,drawWidth,512);}}
    atlas.onload=()=>{const start=performance.now();function frame(now){const elapsed=now-start;const pose=1+Math.floor((elapsed%cycle)/cycle*runCount),slowPose=1+Math.floor(((elapsed/4)%cycle)/cycle*runCount);paint(normal,pose);paint(slow,slowPose);document.querySelector('#frame').textContent=`Full speed pose ${pose}/${runCount}; slow pose ${slowPose}/${runCount}`;requestAnimationFrame(frame);}requestAnimationFrame(frame);};
    </script></html>
    """
    try html.write(toFile: directory + "/game-sheep-run.html", atomically: true, encoding: .utf8)
    try metrics.write(toFile: directory + "/game-sheep-metrics.txt", atomically: true, encoding: .utf8)
}

private func compareAnimation(_ previous: Raster, _ current: Raster) throws {
    let oldColumns = 25, oldSlots = 50, oldRunCount = 48
    guard previous.width == oldColumns * cellWidth && previous.height == 6 * cellHeight,
        current.width == columns * cellWidth && current.height == rows * cellHeight,
        runCount == oldRunCount * 2 else {
        throw BakeError.failed("Animation comparison expects 48-pose 25x6 -> 96-pose 25x12 atlases")
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
    print("48 -> 96: contour mean \(old.meanContourChange) -> \(new.meanContourChange), max \(old.maxContourChange) -> \(new.maxContourChange); boundary max \(old.maxBoundaryStep) -> \(new.maxBoundaryStep) px; centroid max \(old.maxCentroidStep) -> \(new.maxCentroidStep) px; RGB max \(old.maxColorStep) -> \(new.maxColorStep)")
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
    guard args.count == 5 else {
        throw BakeError.failed("usage: build_game_sheep_run.swift SOURCE_4X2.png EXISTING_TOPDOWN_3X1.png OUTPUT.png PREVIEW_DIR")
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
    // Classify original painted pixels before interpolation or rear correction.
    // A moved contour must not change the fleece flood fill.
    let coloredKeys = (0..<coats).map { coat in
        run.map { recolor($0, coat, completeRunFleece: true) }
    }
    run = coloredKeys[0]
    var smooth = [[Raster]](repeating: [], count: coats)
    for key in 0..<keyCount {
        let next = (key + 1) % keyCount, first = run[key], second = run[next]
        let forward = try opticalFlow(first, second), backward = try opticalFlow(second, first)
        let firstMatte = DistanceMatte(first), secondMatte = DistanceMatte(second)
        for coat in 0..<coats {
            smooth[coat].append(coloredKeys[coat][key])
            for part in 1..<samplesPerKey {
                smooth[coat].append(interpolate(coloredKeys[coat][key], coloredKeys[coat][next],
                    forward: forward, backward: backward, firstMatte: firstMatte,
                    secondMatte: secondMatte, t: Float(part) / Float(samplesPerKey)))
            }
        }
        print("Prepared key \(key + 1) -> \(next + 1) with \(samplesPerKey - 1) distinct in-betweens")
        fflush(stdout)
    }
    let rearMetrics = try smoothRearMotion(&smooth, keys: run)
    for coat in 1..<coats { for pose in 0..<runCount {
        smooth[coat][pose] = retainFaceColors(smooth[coat][pose], white: smooth[0][pose])
    } }
    let metrics = try validateProgression(run, run: smooth[0]) + "\n" + rearMetrics
    guard columns * rows == coats * slotsPerCoat else { throw BakeError.failed("Invalid atlas layout") }
    var atlas = Raster(columns * cellWidth, rows * cellHeight), frames: [Raster] = []
    for coat in 0..<coats {
        // Classify fleece only on the original painted keys. Re-running flood
        // fill on a warped in-between can bridge a one-pixel facial shadow and
        // recolour the muzzle for one frame. The SAME white flow, contour and
        // continuous colour weights interpolate all precoloured key sets.
        for pose in 0..<posesPerCoat {
            let painted = pose == 0 ? recolor(standing, coat) : smooth[coat][pose - 1]
            let slot = coat * slotsPerCoat + pose
            atlas.insert(painted, (slot % columns) * cellWidth, (slot / columns) * cellHeight)
            frames.append(painted)
        }
    }
    try validate(frames, original: original, keys: coloredKeys)
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
