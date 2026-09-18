#!/usr/bin/env swift
// Offline cutout rig. Runtime continues to render one ordinary Panda Sprite.
// The source contains six AI-painted parts, not independent generated poses:
// all 72 poses therefore share one face, coat, proportions and silhouette style.
import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

private let cell = 192
private let supersampling = 2
private let runFrames = 48
private let jumpFrames = 24
private let columns = 16
private let rows = 15
private let coatStride = 80
private let strideWorld = 1.0
private let referenceScaleX = 1.05
private let stridePixels = strideWorld / referenceScaleX * Double(cell)
private let stance = 0.24
private let floorY = 144.0 // Hoof paint extends ~2 px below the IK contact.

private struct V {
    var x: Double
    var y: Double
    static func + (a: V, b: V) -> V { V(x: a.x + b.x, y: a.y + b.y) }
    static func - (a: V, b: V) -> V { V(x: a.x - b.x, y: a.y - b.y) }
    static func * (a: V, b: Double) -> V { V(x: a.x * b, y: a.y * b) }
    var length: Double { hypot(x, y) }
}
private func mix(_ a: V, _ b: V, _ t: Double) -> V { a * (1 - t) + b * t }
private func mix(_ a: Double, _ b: Double, _ t: Double) -> Double { a + (b - a) * t }
private func clamp(_ x: Double) -> Double { max(0, min(1, x)) }
private func smooth(_ x: Double) -> Double { let t = clamp(x); return t * t * (3 - 2 * t) }
private func fract(_ x: Double) -> Double { x - floor(x) }
private func rotate(_ v: V, _ angle: Double) -> V {
    V(x: v.x * cos(angle) - v.y * sin(angle), y: v.x * sin(angle) + v.y * cos(angle))
}
private struct Bitmap {
    var width: Int
    var height: Int
    var pixels: [UInt8]
    init(_ width: Int, _ height: Int) {
        self.width = width; self.height = height
        pixels = Array(repeating: 0, count: width * height * 4)
    }
    func offset(_ x: Int, _ y: Int) -> Int { (y * width + x) * 4 }
    func cgImage() -> CGImage {
        CGImage(width: width, height: height, bitsPerComponent: 8, bitsPerPixel: 32,
            bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: CGBitmapInfo.byteOrder32Big.union(CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue)),
            provider: CGDataProvider(data: Data(pixels) as CFData)!, decode: nil,
            shouldInterpolate: true, intent: .defaultIntent)!
    }
}
private enum BakeError: Error { case failure(String) }
private func load(_ path: String) throws -> Bitmap {
    guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil),
          let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
        throw BakeError.failure("Cannot read \(path)")
    }
    var result = Bitmap(image.width, image.height)
    let context = CGContext(data: &result.pixels, width: image.width, height: image.height,
        bitsPerComponent: 8, bytesPerRow: image.width * 4, space: CGColorSpaceCreateDeviceRGB(),
        bitmapInfo: CGBitmapInfo.byteOrder32Big.rawValue | CGImageAlphaInfo.premultipliedLast.rawValue)!
    context.draw(image, in: CGRect(x: 0, y: 0, width: image.width, height: image.height))
    return result
}
private func save(_ bitmap: Bitmap, _ path: String) throws {
    guard let destination = CGImageDestinationCreateWithURL(URL(fileURLWithPath: path) as CFURL,
        UTType.png.identifier as CFString, 1, nil) else { throw BakeError.failure("Cannot write \(path)") }
    CGImageDestinationAddImage(destination, bitmap.cgImage(), nil)
    guard CGImageDestinationFinalize(destination) else { throw BakeError.failure("PNG write failed") }
}

private func adjacent(_ p: Int, _ w: Int, _ h: Int) -> [Int] {
    var n: [Int] = []
    if p % w > 0 { n.append(p - 1) }; if p % w + 1 < w { n.append(p + 1) }
    if p >= w { n.append(p - w) }; if p < w * (h - 1) { n.append(p + w) }
    return n
}
private func extract(_ source: Bitmap, x: Int, y: Int, width: Int, height: Int) throws -> Bitmap {
    var crop = Bitmap(width, height)
    var candidate = Array(repeating: false, count: width * height)
    for py in 0..<height { for px in 0..<width {
        let from = source.offset(x + px, y + py), to = crop.offset(px, py)
        for c in 0..<4 { crop.pixels[to + c] = source.pixels[from + c] }
        let rgb = [Int(crop.pixels[to]), Int(crop.pixels[to + 1]), Int(crop.pixels[to + 2])]
        candidate[py * width + px] = rgb.min()! >= 135 && rgb.max()! - rgb.min()! <= 13
    } }
    // Neutral checkerboard is baked into this source. Remove connected neutral
    // regions touching the cell edge OR containing substantial gray checker
    // squares. Cream wool and enclosed white wool highlights remain intact.
    var visited = Array(repeating: false, count: candidate.count)
    var background = visited
    for start in candidate.indices where candidate[start] && !visited[start] {
        var component = [start], cursor = 0, grays = 0
        var touchesEdge = false
        visited[start] = true
        while cursor < component.count {
            let p = component[cursor]; cursor += 1
            if p % width == 0 || p % width == width - 1 || p < width || p >= width * (height - 1) { touchesEdge = true }
            if crop.pixels[p * 4] < 215 { grays += 1 }
            for q in adjacent(p, width, height) where candidate[q] && !visited[q] {
                visited[q] = true; component.append(q)
            }
        }
        if touchesEdge || (component.count >= 64 && grays * 5 > component.count) {
            for p in component { background[p] = true }
        }
    }
    // Keep the actual painted part, discarding detached anti-aliased checker pixels.
    visited = Array(repeating: false, count: candidate.count)
    var largest: [Int] = []
    for start in background.indices where !background[start] && !visited[start] {
        var component = [start], cursor = 0; visited[start] = true
        while cursor < component.count {
            let p = component[cursor]; cursor += 1
            for q in adjacent(p, width, height) where !background[q] && !visited[q] {
                visited[q] = true; component.append(q)
            }
        }
        if component.count > largest.count { largest = component }
    }
    guard largest.count > 500 else { throw BakeError.failure("Empty part at \(x),\(y)") }
    var keep = Array(repeating: false, count: candidate.count)
    var minX = width, minY = height, maxX = 0, maxY = 0
    for p in largest {
        keep[p] = true; minX = min(minX, p % width); maxX = max(maxX, p % width)
        minY = min(minY, p / width); maxY = max(maxY, p / width)
    }
    var result = Bitmap(maxX - minX + 3, maxY - minY + 3)
    for py in minY...maxY { for px in minX...maxX where keep[py * width + px] {
        let from = crop.offset(px, py), to = result.offset(px - minX + 1, py - minY + 1)
        for c in 0..<4 { result.pixels[to + c] = crop.pixels[from + c] }
    } }
    print("Part \(x),\(y): trimmed \(result.width)x\(result.height), \(largest.count) opaque source pixels")
    return result
}

private func woolWeight(_ r: Double, _ g: Double, _ b: Double) -> Double {
    clamp((r - b - 10) / 34) * clamp((g - 82) / 52) * clamp((r + g + b - 285) / 180)
}
private func recolor(_ source: Bitmap, coat: Int, part: Int? = nil) -> Bitmap {
    guard coat != 0 else { return source }
    var result = source
    let dark = coat == 1 ? [12.0, 11, 14] : [53.0, 27, 16]
    let light = coat == 1 ? [91.0, 87, 85] : [187.0, 119, 62]
    for p in 0..<(source.width * source.height) {
        let o = p * 4, alpha = Double(source.pixels[o + 3]) / 255
        if alpha < 0.001 { continue }
        let rgb = (0..<3).map { min(255, Double(source.pixels[o + $0]) / alpha) }
        let raw = woolWeight(rgb[0], rgb[1], rgb[2])
        let x = Double(p % source.width) / Double(source.width)
        let y = Double(p / source.width) / Double(source.height)
        let wholeWoolPart = part == 0 || part == 2
        // Head art has an explicit wool cap/neck region. Its neutral white
        // highlights are wool too; the white eye and pink ears are not.
        let headWoolRegion = part == 1 && (y < 0.245 || (x < 0.54 && y > 0.42))
        let frontLegWoolRegion = part == 3 && y < 0.31
        let semanticHighlight = (headWoolRegion || frontLegWoolRegion) && rgb[1] > 160
        if !wholeWoolPart && !semanticHighlight && raw <= 0.035 { continue }
        let weight = wholeWoolPart || semanticHighlight ? 1.0 : clamp(pow(raw, 0.52) * 1.24)
        let luma = rgb[0] * 0.2126 + rgb[1] * 0.7152 + rgb[2] * 0.0722
        let shade = pow(clamp((luma - 65) / 190), 1.08)
        for c in 0..<3 {
            result.pixels[o + c] = UInt8(clamp(mix(rgb[c], mix(dark[c], light[c], shade), weight) / 255) * alpha * 255)
        }
    }
    return result
}

// Affine inverse sampling keeps each painted part crisp at arbitrary bone
// angles. Supersampling is spatial only: no crossfaded/ghost limbs or duplicated
// run frames are introduced.
private func draw(_ source: Bitmap, into output: inout Bitmap, center: V,
                  size: V, angle: Double = 0, shade: Double = 1) {
    let ss = Double(supersampling)
    let c = center * ss, bx = rotate(V(x: size.x * ss, y: 0), angle), by = rotate(V(x: 0, y: size.y * ss), angle)
    let corners = [c + bx * -0.5 + by * -0.5, c + bx * 0.5 + by * -0.5,
                   c + bx * 0.5 + by * 0.5, c + bx * -0.5 + by * 0.5]
    let x0 = max(0, Int(floor(corners.map { $0.x }.min()!))), x1 = min(output.width - 1, Int(ceil(corners.map { $0.x }.max()!)))
    let y0 = max(0, Int(floor(corners.map { $0.y }.min()!))), y1 = min(output.height - 1, Int(ceil(corners.map { $0.y }.max()!)))
    if x0 > x1 || y0 > y1 { return }
    let determinant = bx.x * by.y - bx.y * by.x
    for y in y0...y1 { for x in x0...x1 {
        let p = V(x: Double(x) + 0.5, y: Double(y) + 0.5) - c
        let u = (p.x * by.y - p.y * by.x) / determinant + 0.5
        let v = (bx.x * p.y - bx.y * p.x) / determinant + 0.5
        if u < 0 || v < 0 || u >= 1 || v >= 1 { continue }
        let sx = u * Double(source.width - 1), sy = v * Double(source.height - 1)
        let ix = Int(sx), iy = Int(sy), fx = sx - Double(ix), fy = sy - Double(iy)
        let indices = [source.offset(ix, iy), source.offset(min(ix + 1, source.width - 1), iy),
            source.offset(ix, min(iy + 1, source.height - 1)), source.offset(min(ix + 1, source.width - 1), min(iy + 1, source.height - 1))]
        let weights = [(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy]
        var rgba = [Double](repeating: 0, count: 4)
        for i in 0..<4 { for channel in 0..<4 { rgba[channel] += Double(source.pixels[indices[i] + channel]) * weights[i] } }
        let alpha = rgba[3] / 255
        if alpha < 0.001 { continue }
        let out = output.offset(x, y)
        for channel in 0..<4 {
            let value = rgba[channel] * (channel == 3 ? 1 : shade) + Double(output.pixels[out + channel]) * (1 - alpha)
            output.pixels[out + channel] = UInt8(max(0, min(255, value)).rounded())
        }
    } }
}
private func bone(_ source: Bitmap, into output: inout Bitmap, from: V, to: V,
                  proximal: V, distal: V, shade: Double) {
    let sourceA = V(x: proximal.x * Double(source.width), y: proximal.y * Double(source.height))
    let sourceB = V(x: distal.x * Double(source.width), y: distal.y * Double(source.height))
    let sourceBone = sourceB - sourceA, destinationBone = to - from
    let angle = atan2(destinationBone.y, destinationBone.x) - atan2(sourceBone.y, sourceBone.x)
    let scale = destinationBone.length / sourceBone.length
    let midpoint = V(x: Double(source.width) * 0.5, y: Double(source.height) * 0.5)
    let center = from + rotate((midpoint - sourceA) * scale, angle)
    draw(source, into: &output, center: center, size: V(x: Double(source.width) * scale, y: Double(source.height) * scale), angle: angle, shade: shade)
}

private let hips = [V(x: 70, y: 105), V(x: 113, y: 105), V(x: 66, y: 107), V(x: 110, y: 107)]
private let legPhases = [0.11, 0.61, 0.0, 0.48] // hind pair -> fore pair -> suspension
private func foot(_ leg: Int, phase: Double) -> V {
    let q = fract(phase - legPhases[leg]), travel = stridePixels * stance
    let x: Double, lift: Double
    if q < stance {
        // Exactly cancels the character's horizontal travel during contact.
        x = travel * 0.5 - stridePixels * q; lift = 0
    } else {
        let s = (q - stance) / (1 - stance), s2 = s * s, s3 = s2 * s
        let tangent = -stridePixels * (1 - stance)
        x = (2 * s3 - 3 * s2 + 1) * -travel * 0.5 + (s3 - 2 * s2 + s) * tangent
            + (-2 * s3 + 3 * s2) * travel * 0.5 + (s3 - s2) * tangent
        lift = 30 * sin(Double.pi * s)
    }
    return V(x: hips[leg].x + x, y: floorY - lift - (leg < 2 ? 1 : 0))
}
private struct Pose {
    var bodyY: Double
    var pitch: Double
    var stretchX: Double
    var stretchY: Double
    var head: Double
    var tail: Double
    var feet: [V]
}
private func runPose(_ phase: Double) -> Pose {
    let a = phase * Double.pi * 2
    return Pose(bodyY: 94 + 2.1 * cos(2 * (a - Double.pi * 2 * 0.165)),
        pitch: -0.035 * sin(a - 0.63), stretchX: 1 + 0.014 * sin(a), stretchY: 1 - 0.018 * sin(a),
        head: 0.027 * sin(a - 0.63) + 0.021 * sin(a * 2 + 0.8), tail: 0.13 * sin(a + 0.7),
        feet: (0..<4).map { foot($0, phase: phase) })
}
private func blendPose(_ a: Pose, _ b: Pose, _ t: Double) -> Pose {
    Pose(bodyY: mix(a.bodyY, b.bodyY, t), pitch: mix(a.pitch, b.pitch, t),
        stretchX: mix(a.stretchX, b.stretchX, t), stretchY: mix(a.stretchY, b.stretchY, t),
        head: mix(a.head, b.head, t), tail: mix(a.tail, b.tail, t),
        feet: zip(a.feet, b.feet).map { mix($0, $1, t) })
}
private func airPose(_ t: Double) -> Pose {
    let tuck = pow(sin(Double.pi * t), 0.85)
    var feet: [V] = []
    for leg in 0..<4 {
        let front = leg % 2 == 1
        let start = V(x: hips[leg].x - (front ? 1 : 9), y: floorY - (leg < 2 ? 1 : 0))
        let end = V(x: hips[leg].x + (front ? 21 : -11), y: floorY - (front ? 0 : 7))
        var position = mix(start, end, smooth(t))
        position.y -= (front ? 23 : 19) * tuck
        position.x += (front ? -10 : 13) * tuck
        feet.append(position)
    }
    return Pose(bodyY: 95 - 2.5 * tuck, pitch: mix(-0.07, 0.07, smooth(t)),
        stretchX: 1 + 0.018 * tuck, stretchY: 1 - 0.022 * tuck,
        head: -0.04 * sin(Double.pi * t), tail: -0.10 + 0.16 * t, feet: feet)
}
private func jumpPose(_ frame: Int) -> Pose {
    if frame <= 4 { return blendPose(runPose(0), airPose(0), smooth(Double(frame) / 4)) }
    if frame <= 20 { return airPose(Double(frame - 4) / 16) }
    return blendPose(airPose(1), runPose(0), smooth(Double(frame - 20) / 3))
}
private func render(_ pose: Pose, parts: [Bitmap]) -> Bitmap {
    var output = Bitmap(cell * supersampling, cell * supersampling)
    let body = V(x: 89, y: pose.bodyY)
    func onBody(_ v: V) -> V {
        let local = v - V(x: 89, y: 94)
        return body + rotate(V(x: local.x * pose.stretchX, y: local.y * pose.stretchY), pose.pitch)
    }
    // Far legs are darker and start slightly higher. Real two-bone IK bends
    // hind hocks backward, fore knees forward; feet never orbit like pedals.
    for leg in 0..<4 {
        let hip = onBody(hips[leg]), requestedFoot = pose.feet[leg]
        let delta = requestedFoot - hip, distance = min(48.95, max(4, delta.length))
        let direction = delta * (1 / max(0.001, delta.length)), target = hip + direction * distance
        let l1 = 25.0, l2 = 24.0, along = (l1 * l1 - l2 * l2 + distance * distance) / (2 * distance)
        let side = sqrt(max(0, l1 * l1 - along * along)) * (leg % 2 == 1 ? -1.0 : 1.0)
        let knee = hip + direction * along + V(x: -direction.y, y: direction.x) * side
        let shade = leg < 2 ? 0.73 : 1.0
        bone(parts[leg % 2 == 1 ? 3 : 4], into: &output, from: hip, to: knee,
             proximal: V(x: 0.50, y: 0.14), distal: V(x: 0.50, y: 0.91), shade: shade)
        bone(parts[5], into: &output, from: knee, to: target,
             proximal: V(x: 0.42, y: 0.15), distal: V(x: 0.60, y: 0.96), shade: shade)
    }
    draw(parts[2], into: &output, center: onBody(V(x: 42, y: 85)), size: V(x: 23, y: 12), angle: pose.pitch + pose.tail)
    draw(parts[0], into: &output, center: body,
         size: V(x: 86 * pose.stretchX, y: 51 * pose.stretchY), angle: pose.pitch)
    draw(parts[1], into: &output, center: onBody(V(x: 127, y: 84)),
         size: V(x: 48, y: 48), angle: pose.pitch + pose.head)
    var small = Bitmap(cell, cell)
    for y in 0..<cell { for x in 0..<cell { for c in 0..<4 {
        var sum = 0
        for oy in 0..<supersampling { for ox in 0..<supersampling {
            sum += Int(output.pixels[output.offset(x * supersampling + ox, y * supersampling + oy) + c])
        } }
        small.pixels[small.offset(x, y) + c] = UInt8(sum / (supersampling * supersampling))
    } } }
    return small
}
private func copy(_ bitmap: Bitmap, to output: inout Bitmap, x: Int, y: Int) {
    for row in 0..<bitmap.height {
        let from = row * bitmap.width * 4, to = output.offset(x, y + row)
        output.pixels.replaceSubrange(to..<(to + bitmap.width * 4), with: bitmap.pixels[from..<(from + bitmap.width * 4)])
    }
}
private func preview(_ frame: Bitmap, coat: Int) -> Bitmap {
    var result = Bitmap(cell, cell)
    for y in 0..<cell { for x in 0..<cell {
        let o = result.offset(x, y), base = y < 147 ? [106, 162, 176] : [102, 130, 65]
        let a = Double(frame.pixels[o + 3]) / 255
        for c in 0..<3 { result.pixels[o + c] = UInt8(min(255, Double(frame.pixels[o + c]) + Double(base[c]) * (1 - a))) }
        result.pixels[o + 3] = 255
    } }
    return result
}
private func writeGIF(_ frames: [Bitmap], path: String, delay: Double) throws {
    guard let destination = CGImageDestinationCreateWithURL(URL(fileURLWithPath: path) as CFURL,
        UTType.gif.identifier as CFString, frames.count, nil) else { throw BakeError.failure("Cannot write GIF") }
    CGImageDestinationSetProperties(destination, [kCGImagePropertyGIFDictionary: [kCGImagePropertyGIFLoopCount: 0]] as CFDictionary)
    for (index, frame) in frames.enumerated() {
        // GIF has centisecond timing. Distribute rounding over the full cycle
        // so inspection previews do not suggest a different running speed.
        let start = (Double(index) * delay * 100).rounded()
        let end = (Double(index + 1) * delay * 100).rounded()
        let frameDelay = max(2, end - start) / 100
        CGImageDestinationAddImage(destination, frame.cgImage(), [kCGImagePropertyGIFDictionary:
            [kCGImagePropertyGIFDelayTime: frameDelay, kCGImagePropertyGIFUnclampedDelayTime: frameDelay]] as CFDictionary)
    }
    guard CGImageDestinationFinalize(destination) else { throw BakeError.failure("GIF write failed") }
}
private func difference(_ a: Bitmap, _ b: Bitmap) -> Double {
    var sum = 0.0
    for p in a.pixels.indices { sum += abs(Double(a.pixels[p]) - Double(b.pixels[p])) }
    return sum / Double(a.pixels.count)
}
private func validate(_ run: [Bitmap], _ jump: [Bitmap]) throws {
    var minX = cell, maxX = 0, minY = cell, maxY = 0
    var hashes = Set<Data>()
    for (i, frame) in (run + jump).enumerated() {
        if i < runFrames { hashes.insert(Data(frame.pixels)) }
        for y in 0..<cell { for x in 0..<cell where frame.pixels[frame.offset(x, y) + 3] > 12 {
            minX = min(minX, x); maxX = max(maxX, x); minY = min(minY, y); maxY = max(maxY, y)
        } }
    }
    guard hashes.count == runFrames else { throw BakeError.failure("Run frames are not all unique") }
    guard minX >= 28 && maxX <= 164 && minY >= 39 && maxY <= 153 else {
        throw BakeError.failure("Alpha escaped tested runtime bounds: \(minX)...\(maxX),\(minY)...\(maxY)")
    }
    let differences = (0..<runFrames).map { difference(run[$0], run[($0 + 1) % runFrames]) }
    guard differences.last! <= differences.dropLast().max()! * 1.25,
          difference(jump[0], run[0]) == 0, difference(jump[23], run[0]) == 0 else {
        throw BakeError.failure("Animation seam mismatch")
    }
    // A stance hoof's local velocity must exactly cancel sprite translation.
    let dt = 0.0001
    var maxStanceReach = 0.0
    for leg in 0..<4 {
        let p = legPhases[leg] + stance * 0.5
        let velocity = (foot(leg, phase: p + dt).x - foot(leg, phase: p).x) / dt
        guard abs(velocity + stridePixels) < 0.0001 else { throw BakeError.failure("Sliding stance hoof") }
        for sample in 0...1000 {
            let phase = legPhases[leg] + stance * Double(sample) / 1000
            let pose = runPose(phase)
            let local = hips[leg] - V(x: 89, y: 94)
            let hip = V(x: 89, y: pose.bodyY) + rotate(V(x: local.x * pose.stretchX, y: local.y * pose.stretchY), pose.pitch)
            let reach = (pose.feet[leg] - hip).length
            maxStanceReach = max(maxStanceReach, reach)
            guard reach < 48.95 else { throw BakeError.failure("Planted foot clamped by IK: \(reach)") }
        }
    }
    print("Validation: \(hashes.count) unique run poses, alpha x\(minX)...\(maxX) y\(minY)...\(maxY)")
    print("Adjacent RGBA mean difference \(differences.min()!)...\(differences.max()!), wrap \(differences.last!)")
    print("Jump first/last = run phase zero exactly; stance hoof slope = -\(stridePixels) px/cycle")
    print("Actual stance IK max reach \(maxStanceReach) < 48.95; no contact clamping/sliding")
}
private func main() throws {
    guard CommandLine.arguments.count == 4 else {
        throw BakeError.failure("usage: build_sheep_rig.swift PARTS.png OUTPUT.png PREVIEW_DIRECTORY")
    }
    let sourcePath = CommandLine.arguments[1], outputPath = CommandLine.arguments[2], previewDir = CommandLine.arguments[3]
    let source = try load(sourcePath)
    guard source.width == 1536 && source.height == 1024 else { throw BakeError.failure("Expected 1536x1024 parts sheet") }
    let rects = [(0, 0, 620, 512), (630, 0, 450, 512), (1110, 0, 426, 512),
                 (0, 512, 512, 512), (512, 512, 512, 512), (1024, 512, 512, 512)]
    let parts = try rects.map { try extract(source, x: $0.0, y: $0.1, width: $0.2, height: $0.3) }
    try FileManager.default.createDirectory(atPath: previewDir, withIntermediateDirectories: true)
    for i in 0..<parts.count { try save(parts[i], "\(previewDir)/part-\(i).png") }
    let run = (0..<runFrames).map { render(runPose(Double($0) / Double(runFrames)), parts: parts) }
    let jump = (0..<jumpFrames).map { render(jumpPose($0), parts: parts) }
    try validate(run, jump)
    var atlas = Bitmap(columns * cell, rows * cell)
    var coatedFrames: [[Bitmap]] = []
    for coat in 0..<3 {
        let coatParts = parts.enumerated().map { recolor($0.element, coat: coat, part: $0.offset) }
        let coatRun = coat == 0 ? run : (0..<runFrames).map { render(runPose(Double($0) / Double(runFrames)), parts: coatParts) }
        let coatJump = coat == 0 ? jump : (0..<jumpFrames).map { render(jumpPose($0), parts: coatParts) }
        let frames = coatRun + coatJump
        coatedFrames.append(frames)
        for (pose, frame) in (run + jump).enumerated() {
            let recolored = frames[pose], index = coat * coatStride + pose
            for p in 0..<(cell * cell) where recolored.pixels[p * 4 + 3] != frame.pixels[p * 4 + 3] {
                throw BakeError.failure("Coat silhouette changed at \(p)")
            }
            copy(recolored, to: &atlas, x: (index % columns) * cell, y: (index / columns) * cell)
        }
    }
    try save(atlas, outputPath)
    var sheet = Bitmap(8 * cell, 6 * cell)
    for i in 0..<runFrames { copy(preview(run[i], coat: 0), to: &sheet, x: (i % 8) * cell, y: (i / 8) * cell) }
    try save(sheet, "\(previewDir)/run-contact-sheet.png")
    var jumps = Bitmap(8 * cell, 3 * cell)
    for i in 0..<jumpFrames { copy(preview(jump[i], coat: 0), to: &jumps, x: (i % 8) * cell, y: (i / 8) * cell) }
    try save(jumps, "\(previewDir)/jump-contact-sheet.png")
    var coats = Bitmap(3 * cell, cell)
    for coat in 0..<3 { copy(preview(coatedFrames[coat][12], coat: coat), to: &coats, x: coat * cell, y: 0) }
    try save(coats, "\(previewDir)/coats.png")
    // The runtime atlas keeps all 48 poses. The GIF uses every second pose to
    // avoid sub-20ms frames that viewers may clamp into apparent slow motion.
    let gifFrames = stride(from: 0, to: runFrames, by: 2).map { preview(run[$0], coat: 0) }
    try writeGIF(gifFrames, path: "\(previewDir)/run.gif", delay: strideWorld / 1.75 / Double(gifFrames.count))
    print("Wrote \(outputPath): \(atlas.width)x\(atlas.height), 16x15, stride80, 48 run +24 jump per coat")
    print("Runtime stride MUST be \(strideWorld) world unit (reference scaleX \(referenceScaleX)); preview \(previewDir)/run.gif")
}
do { try main() } catch { fputs("\(error)\n", stderr); exit(1) }
