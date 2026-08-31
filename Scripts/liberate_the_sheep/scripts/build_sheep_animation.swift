#!/usr/bin/env swift

import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

private struct Bitmap {
    let width: Int
    let height: Int
    var pixels: [UInt8]

    func offset(x: Int, y: Int) -> Int {
        (y * width + x) * 4
    }
}

private enum BuildError: Error, CustomStringConvertible {
    case usage
    case cannotLoad(String)
    case invalidDimensions(String)
    case cannotWrite(String)

    var description: String {
        switch self {
        case .usage:
            return "usage: build_sheep_animation.swift RUN_4X4.png JUMP_6X4.png TOPDOWN_WHITE.png SIDE_OUTPUT.png TOPDOWN_OUTPUT.png"
        case let .cannotLoad(path):
            return "cannot load PNG: \(path)"
        case let .invalidDimensions(path):
            return "source has invalid dimensions or contains no usable sheep: \(path)"
        case let .cannotWrite(path):
            return "cannot write PNG: \(path)"
        }
    }
}

private func loadBitmap(path: String) throws -> Bitmap {
    let url = URL(fileURLWithPath: path) as CFURL
    guard
        let source = CGImageSourceCreateWithURL(url, nil),
        let image = CGImageSourceCreateImageAtIndex(source, 0, nil)
    else {
        throw BuildError.cannotLoad(path)
    }

    let width = image.width
    let height = image.height
    var pixels = [UInt8](repeating: 0, count: width * height * 4)
    let colorSpace = CGColorSpaceCreateDeviceRGB()
    let bitmapInfo = CGBitmapInfo.byteOrder32Big.rawValue |
        CGImageAlphaInfo.premultipliedLast.rawValue
    guard let context = CGContext(
        data: &pixels,
        width: width,
        height: height,
        bitsPerComponent: 8,
        bytesPerRow: width * 4,
        space: colorSpace,
        bitmapInfo: bitmapInfo
    ) else {
        throw BuildError.cannotLoad(path)
    }

    // ImageIO and the bitmap context preserve the source scanline order here;
    // keeping the draw untransformed also keeps Panda's atlas rows upright.
    context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
    return Bitmap(width: width, height: height, pixels: pixels)
}

private func writePNG(_ bitmap: Bitmap, path: String) throws {
    let data = Data(bitmap.pixels) as CFData
    guard let provider = CGDataProvider(data: data) else {
        throw BuildError.cannotWrite(path)
    }
    let colorSpace = CGColorSpaceCreateDeviceRGB()
    let bitmapInfo = CGBitmapInfo.byteOrder32Big.union(
        CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue)
    )
    guard let image = CGImage(
        width: bitmap.width,
        height: bitmap.height,
        bitsPerComponent: 8,
        bitsPerPixel: 32,
        bytesPerRow: bitmap.width * 4,
        space: colorSpace,
        bitmapInfo: bitmapInfo,
        provider: provider,
        decode: nil,
        shouldInterpolate: true,
        intent: .defaultIntent
    ) else {
        throw BuildError.cannotWrite(path)
    }

    let outputURL = URL(fileURLWithPath: path) as CFURL
    guard let destination = CGImageDestinationCreateWithURL(
        outputURL,
        UTType.png.identifier as CFString,
        1,
        nil
    ) else {
        throw BuildError.cannotWrite(path)
    }
    CGImageDestinationAddImage(destination, image, nil)
    guard CGImageDestinationFinalize(destination) else {
        throw BuildError.cannotWrite(path)
    }
}

private let runSourceColumns = 4
private let runSourceRows = 4
private let runFrameCount = 16
private let jumpSourceColumns = 6
private let jumpSourceRows = 4
private let jumpFrameCount = 24

// Each coat owns three 16-cell rows: run, jump 0...15, jump 16...23.
// The last eight cells of the third row are deliberately transparent.
private let coatCount = 3
private let sideOutputColumns = 16
private let sideRowsPerCoat = 3
private let sideOutputRows = coatCount * sideRowsPerCoat
private let sideOutputCellSize = 192
private let sideCoatStride = sideOutputColumns * sideRowsPerCoat

private let topDownOutputColumns = 3
private let topDownOutputCellWidth = 512
private let topDownOutputCellHeight = 1024

private func neighbors(of index: Int, width: Int, height: Int) -> [Int] {
    let x = index % width
    let y = index / width
    var result: [Int] = []
    result.reserveCapacity(4)
    if x > 0 { result.append(index - 1) }
    if x + 1 < width { result.append(index + 1) }
    if y > 0 { result.append(index - width) }
    if y + 1 < height { result.append(index + width) }
    return result
}

private func largestComponent(mask: [Bool], width: Int, height: Int) -> [Bool] {
    var visited = [Bool](repeating: false, count: mask.count)
    var largest: [Int] = []

    for start in mask.indices where mask[start] && !visited[start] {
        var queue = [start]
        var cursor = 0
        var component: [Int] = []
        visited[start] = true
        while cursor < queue.count {
            let current = queue[cursor]
            cursor += 1
            component.append(current)
            for next in neighbors(of: current, width: width, height: height)
                where mask[next] && !visited[next]
            {
                visited[next] = true
                queue.append(next)
            }
        }
        if component.count > largest.count {
            largest = component
        }
    }

    var result = [Bool](repeating: false, count: mask.count)
    for index in largest {
        result[index] = true
    }
    return result
}

private func cleanRunFrame(
    source: Bitmap,
    originX: Int,
    originY: Int,
    width: Int,
    height: Int
) -> Bitmap {
    var frame = Bitmap(
        width: width,
        height: height,
        pixels: [UInt8](repeating: 0, count: width * height * 4)
    )
    var backgroundCandidate = [Bool](repeating: false, count: width * height)

    for y in 0..<height {
        for x in 0..<width {
            let sourceOffset = source.offset(x: originX + x, y: originY + y)
            let frameOffset = frame.offset(x: x, y: y)
            let red = source.pixels[sourceOffset]
            let green = source.pixels[sourceOffset + 1]
            let blue = source.pixels[sourceOffset + 2]
            frame.pixels[frameOffset] = red
            frame.pixels[frameOffset + 1] = green
            frame.pixels[frameOffset + 2] = blue
            frame.pixels[frameOffset + 3] = 255

            let minimum = min(red, min(green, blue))
            let maximum = max(red, max(green, blue))
            backgroundCandidate[y * width + x] =
                minimum >= 225 && Int(maximum) - Int(minimum) <= 12
        }
    }

    // The generated source sometimes bakes a neutral checkerboard into RGB.
    // Flood only through checker-like pixels reachable from a cell edge. Pale
    // wool enclosed by the sheep silhouette is therefore kept intact.
    var exterior = [Bool](repeating: false, count: width * height)
    var queue: [Int] = []
    func enqueue(_ index: Int) {
        if backgroundCandidate[index] && !exterior[index] {
            exterior[index] = true
            queue.append(index)
        }
    }
    for x in 0..<width {
        enqueue(x)
        enqueue((height - 1) * width + x)
    }
    for y in 0..<height {
        enqueue(y * width)
        enqueue(y * width + width - 1)
    }
    var cursor = 0
    while cursor < queue.count {
        let current = queue[cursor]
        cursor += 1
        for next in neighbors(of: current, width: width, height: height)
            where backgroundCandidate[next] && !exterior[next]
        {
            exterior[next] = true
            queue.append(next)
        }
    }

    let initialMask = exterior.map { !$0 }
    let keep = largestComponent(mask: initialMask, width: width, height: height)
    for index in keep.indices {
        let offset = index * 4
        if keep[index] {
            frame.pixels[offset + 3] = 255
        } else {
            frame.pixels[offset] = 0
            frame.pixels[offset + 1] = 0
            frame.pixels[offset + 2] = 0
            frame.pixels[offset + 3] = 0
        }
    }
    return frame
}

private func cleanAlphaFrame(
    source: Bitmap,
    originX: Int,
    originY: Int,
    width: Int,
    height: Int
) -> Bitmap {
    var frame = Bitmap(
        width: width,
        height: height,
        pixels: [UInt8](repeating: 0, count: width * height * 4)
    )
    var mask = [Bool](repeating: false, count: width * height)
    for y in 0..<height {
        for x in 0..<width {
            let sourceOffset = source.offset(x: originX + x, y: originY + y)
            let frameOffset = frame.offset(x: x, y: y)
            for channel in 0..<4 {
                frame.pixels[frameOffset + channel] = source.pixels[sourceOffset + channel]
            }
            mask[y * width + x] = source.pixels[sourceOffset + 3] > 12
        }
    }

    // Keep the connected character and discard the generator's isolated
    // colored specks. Every authored pose is a single connected silhouette.
    let keep = largestComponent(mask: mask, width: width, height: height)
    for index in keep.indices where !keep[index] {
        let offset = index * 4
        frame.pixels[offset] = 0
        frame.pixels[offset + 1] = 0
        frame.pixels[offset + 2] = 0
        frame.pixels[offset + 3] = 0
    }
    return frame
}

private func sampleBilinear(
    _ source: Bitmap,
    x: Double,
    y: Double,
    channel: Int
) -> UInt8 {
    let x0 = max(0, min(source.width - 1, Int(floor(x))))
    let y0 = max(0, min(source.height - 1, Int(floor(y))))
    let x1 = min(source.width - 1, x0 + 1)
    let y1 = min(source.height - 1, y0 + 1)
    let tx = x - Double(x0)
    let ty = y - Double(y0)
    func value(_ px: Int, _ py: Int) -> Double {
        Double(source.pixels[source.offset(x: px, y: py) + channel])
    }
    let top = value(x0, y0) * (1.0 - tx) + value(x1, y0) * tx
    let bottom = value(x0, y1) * (1.0 - tx) + value(x1, y1) * tx
    return UInt8(max(0.0, min(255.0, top * (1.0 - ty) + bottom * ty)).rounded())
}

private struct FrameMetrics {
    let anchorX: Double
    let anchorY: Double
    let torsoWidth: Double
    let minimumX: Int
    let minimumY: Int
    let maximumX: Int
    let maximumY: Int
}

private func clamp01(_ value: Double) -> Double {
    max(0.0, min(1.0, value))
}

private func unpremultipliedColor(
    _ bitmap: Bitmap,
    offset: Int
) -> (red: Double, green: Double, blue: Double, alpha: Double) {
    let alpha = Double(bitmap.pixels[offset + 3]) / 255.0
    guard alpha > 0.0001 else {
        return (0.0, 0.0, 0.0, 0.0)
    }
    return (
        min(255.0, Double(bitmap.pixels[offset]) / alpha),
        min(255.0, Double(bitmap.pixels[offset + 1]) / alpha),
        min(255.0, Double(bitmap.pixels[offset + 2]) / alpha),
        alpha
    )
}

// Warm, bright pixels are wool. Neutral eye highlights and the charcoal face
// have little red/blue separation; brown hooves and pink ears are too dark in
// green. This keeps coat recoloring away from facial features and legs.
private func woolWeight(red: Double, green: Double, blue: Double) -> Double {
    let warmth = clamp01((red - blue - 10.0) / 34.0)
    let greenFloor = clamp01((green - 82.0) / 52.0)
    let brightness = clamp01((red + green + blue - 285.0) / 180.0)
    return warmth * greenFloor * brightness
}

private func metrics(for frame: Bitmap) -> FrameMetrics? {
    var weightedX = 0.0
    var weightedY = 0.0
    var totalWeight = 0.0
    var woolMinimumX = frame.width
    var woolMaximumX = -1
    var minimumX = frame.width
    var minimumY = frame.height
    var maximumX = -1
    var maximumY = -1
    var alphaX = 0.0
    var alphaY = 0.0
    var alphaWeight = 0.0

    for y in 0..<frame.height {
        for x in 0..<frame.width {
            let offset = frame.offset(x: x, y: y)
            let color = unpremultipliedColor(frame, offset: offset)
            guard color.alpha > 0.05 else { continue }
            minimumX = min(minimumX, x)
            minimumY = min(minimumY, y)
            maximumX = max(maximumX, x)
            maximumY = max(maximumY, y)
            alphaX += Double(x) * color.alpha
            alphaY += Double(y) * color.alpha
            alphaWeight += color.alpha

            let weight = woolWeight(
                red: color.red,
                green: color.green,
                blue: color.blue
            ) * color.alpha
            if weight > 0.16 {
                weightedX += Double(x) * weight
                weightedY += Double(y) * weight
                totalWeight += weight
                woolMinimumX = min(woolMinimumX, x)
                woolMaximumX = max(woolMaximumX, x)
            }
        }
    }

    guard maximumX >= minimumX, maximumY >= minimumY, alphaWeight > 0.0 else {
        return nil
    }
    let hasWool = totalWeight > 20.0 && woolMaximumX >= woolMinimumX
    let anchorX = hasWool ? weightedX / totalWeight : alphaX / alphaWeight
    let anchorY = hasWool ? weightedY / totalWeight : alphaY / alphaWeight
    let fallbackWidth = Double(maximumX - minimumX + 1) * 0.68
    let torsoWidth = hasWool
        ? Double(woolMaximumX - woolMinimumX + 1)
        : max(1.0, fallbackWidth)
    return FrameMetrics(
        anchorX: anchorX,
        anchorY: anchorY,
        torsoWidth: max(1.0, torsoWidth),
        minimumX: minimumX,
        minimumY: minimumY,
        maximumX: maximumX,
        maximumY: maximumY
    )
}

private func gridFrame(
    source: Bitmap,
    index: Int,
    columns: Int,
    rows: Int,
    sourcePath: String
) throws -> Bitmap {
    guard index >= 0, index < columns * rows, columns > 0, rows > 0 else {
        throw BuildError.invalidDimensions(sourcePath)
    }
    let column = index % columns
    let row = index / columns
    let originX = column * source.width / columns
    let endX = (column + 1) * source.width / columns
    let originY = row * source.height / rows
    let endY = (row + 1) * source.height / rows
    guard endX > originX, endY > originY else {
        throw BuildError.invalidDimensions(sourcePath)
    }
    let frame = cleanRunFrame(
        source: source,
        originX: originX,
        originY: originY,
        width: endX - originX,
        height: endY - originY
    )
    guard metrics(for: frame) != nil else {
        throw BuildError.invalidDimensions(sourcePath)
    }
    return frame
}

private func normalizedSideFrame(_ frame: Bitmap) throws -> Bitmap {
    guard let frameMetrics = metrics(for: frame) else {
        throw BuildError.invalidDimensions("generated frame")
    }
    let targetTorsoWidth = 118.0
    let maximumExtent = Double(sideOutputCellSize - 14)
    let silhouetteWidth = Double(frameMetrics.maximumX - frameMetrics.minimumX + 1)
    let silhouetteHeight = Double(frameMetrics.maximumY - frameMetrics.minimumY + 1)
    var scale = targetTorsoWidth / frameMetrics.torsoWidth
    scale = min(
        scale,
        maximumExtent / max(1.0, silhouetteWidth),
        maximumExtent / max(1.0, silhouetteHeight)
    )

    var output = Bitmap(
        width: sideOutputCellSize,
        height: sideOutputCellSize,
        pixels: [UInt8](repeating: 0, count: sideOutputCellSize * sideOutputCellSize * 4)
    )
    let target = Double(sideOutputCellSize) * 0.5
    for y in 0..<sideOutputCellSize {
        let sourceY = frameMetrics.anchorY + (Double(y) + 0.5 - target) / scale
        guard sourceY >= 0.0, sourceY <= Double(frame.height - 1) else { continue }
        for x in 0..<sideOutputCellSize {
            let sourceX = frameMetrics.anchorX + (Double(x) + 0.5 - target) / scale
            guard sourceX >= 0.0, sourceX <= Double(frame.width - 1) else { continue }
            let outputOffset = output.offset(x: x, y: y)
            for channel in 0..<4 {
                output.pixels[outputOffset + channel] = sampleBilinear(
                    frame,
                    x: sourceX,
                    y: sourceY,
                    channel: channel
                )
            }
        }
    }
    return output
}

private func recolored(_ source: Bitmap, coat: Int) -> Bitmap {
    guard coat != 0 else { return source }
    var output = source
    let dark: (Double, Double, Double)
    let light: (Double, Double, Double)
    if coat == 1 {
        dark = (12.0, 11.0, 14.0)
        light = (91.0, 87.0, 85.0)
    } else {
        dark = (53.0, 27.0, 16.0)
        light = (187.0, 119.0, 62.0)
    }

    for index in 0..<(source.width * source.height) {
        let offset = index * 4
        let color = unpremultipliedColor(source, offset: offset)
        guard color.alpha > 0.001 else { continue }
        let rawWeight = woolWeight(red: color.red, green: color.green, blue: color.blue)
        guard rawWeight > 0.035 else { continue }
        let weight = clamp01(pow(rawWeight, 0.52) * 1.24)
        let luminance =
            color.red * 0.2126 + color.green * 0.7152 + color.blue * 0.0722
        let shade = pow(clamp01((luminance - 65.0) / 190.0), 1.08)
        let mappedRed = dark.0 + (light.0 - dark.0) * shade
        let mappedGreen = dark.1 + (light.1 - dark.1) * shade
        let mappedBlue = dark.2 + (light.2 - dark.2) * shade
        let red = color.red + (mappedRed - color.red) * weight
        let green = color.green + (mappedGreen - color.green) * weight
        let blue = color.blue + (mappedBlue - color.blue) * weight
        output.pixels[offset] = UInt8(clamp01(red / 255.0) * color.alpha * 255.0)
        output.pixels[offset + 1] = UInt8(clamp01(green / 255.0) * color.alpha * 255.0)
        output.pixels[offset + 2] = UInt8(clamp01(blue / 255.0) * color.alpha * 255.0)
    }
    return output
}

private func copyCell(
    _ cell: Bitmap,
    index: Int,
    columns: Int,
    output: inout Bitmap
) {
    let originX = (index % columns) * cell.width
    let originY = (index / columns) * cell.height
    precondition(originX + cell.width <= output.width)
    precondition(originY + cell.height <= output.height)
    for y in 0..<cell.height {
        let sourceStart = cell.offset(x: 0, y: y)
        let destinationStart = output.offset(x: originX, y: originY + y)
        output.pixels.replaceSubrange(
            destinationStart..<(destinationStart + cell.width * 4),
            with: cell.pixels[sourceStart..<(sourceStart + cell.width * 4)]
        )
    }
}

private func fittedTopDownFrame(_ source: Bitmap) -> Bitmap {
    let margin = 12.0
    let scale = min(
        (Double(topDownOutputCellWidth) - margin * 2.0) / Double(source.width),
        (Double(topDownOutputCellHeight) - margin * 2.0) / Double(source.height)
    )
    let drawWidth = max(1, Int((Double(source.width) * scale).rounded()))
    let drawHeight = max(1, Int((Double(source.height) * scale).rounded()))
    let originX = (topDownOutputCellWidth - drawWidth) / 2
    let originY = (topDownOutputCellHeight - drawHeight) / 2
    var output = Bitmap(
        width: topDownOutputCellWidth,
        height: topDownOutputCellHeight,
        pixels: [UInt8](
            repeating: 0,
            count: topDownOutputCellWidth * topDownOutputCellHeight * 4
        )
    )
    for y in 0..<drawHeight {
        let sourceY = (Double(y) + 0.5) / scale - 0.5
        for x in 0..<drawWidth {
            let sourceX = (Double(x) + 0.5) / scale - 0.5
            let outputOffset = output.offset(x: originX + x, y: originY + y)
            for channel in 0..<4 {
                output.pixels[outputOffset + channel] = sampleBilinear(
                    source,
                    x: sourceX,
                    y: sourceY,
                    channel: channel
                )
            }
        }
    }
    return output
}

private func printAlignmentSummary(_ frames: [Bitmap], label: String) {
    let values = frames.compactMap { metrics(for: $0) }
    guard
        let minimumX = values.map(\.anchorX).min(),
        let maximumX = values.map(\.anchorX).max(),
        let minimumY = values.map(\.anchorY).min(),
        let maximumY = values.map(\.anchorY).max(),
        let minimumWidth = values.map(\.torsoWidth).min(),
        let maximumWidth = values.map(\.torsoWidth).max()
    else {
        return
    }
    print(
        String(
            format: "%@: anchor x %.2f...%.2f, y %.2f...%.2f, torso %.1f...%.1f px",
            label,
            minimumX,
            maximumX,
            minimumY,
            maximumY,
            minimumWidth,
            maximumWidth
        )
    )
}

private func build(
    runPath: String,
    jumpPath: String,
    topDownPath: String,
    sideOutputPath: String,
    topDownOutputPath: String
) throws {
    let runSource = try loadBitmap(path: runPath)
    let jumpSource = try loadBitmap(path: jumpPath)
    let topDownSource = try loadBitmap(path: topDownPath)

    var runFrames: [Bitmap] = []
    runFrames.reserveCapacity(runFrameCount)
    for index in 0..<runFrameCount {
        let frame = try gridFrame(
            source: runSource,
            index: index,
            columns: runSourceColumns,
            rows: runSourceRows,
            sourcePath: runPath
        )
        runFrames.append(try normalizedSideFrame(frame))
    }

    var jumpFrames: [Bitmap] = []
    jumpFrames.reserveCapacity(jumpFrameCount)
    for index in 0..<jumpFrameCount {
        let frame = try gridFrame(
            source: jumpSource,
            index: index,
            columns: jumpSourceColumns,
            rows: jumpSourceRows,
            sourcePath: jumpPath
        )
        jumpFrames.append(try normalizedSideFrame(frame))
    }
    // Pixel-identical transitions avoid a pop at both edges of the jump state.
    jumpFrames[0] = runFrames[0]
    jumpFrames[jumpFrameCount - 1] = runFrames[0]
    printAlignmentSummary(runFrames, label: "run")
    printAlignmentSummary(jumpFrames, label: "jump")

    var sideOutput = Bitmap(
        width: sideOutputColumns * sideOutputCellSize,
        height: sideOutputRows * sideOutputCellSize,
        pixels: [UInt8](
            repeating: 0,
            count: sideOutputColumns * sideOutputRows *
                sideOutputCellSize * sideOutputCellSize * 4
        )
    )
    for coat in 0..<coatCount {
        let coatBase = coat * sideCoatStride
        for index in 0..<runFrameCount {
            copyCell(
                recolored(runFrames[index], coat: coat),
                index: coatBase + index,
                columns: sideOutputColumns,
                output: &sideOutput
            )
        }
        for index in 0..<jumpFrameCount {
            copyCell(
                recolored(jumpFrames[index], coat: coat),
                index: coatBase + runFrameCount + index,
                columns: sideOutputColumns,
                output: &sideOutput
            )
        }
    }
    try writePNG(sideOutput, path: sideOutputPath)

    let cleanTopDown = cleanAlphaFrame(
        source: topDownSource,
        originX: 0,
        originY: 0,
        width: topDownSource.width,
        height: topDownSource.height
    )
    let fittedTopDown = fittedTopDownFrame(cleanTopDown)
    var topDownOutput = Bitmap(
        width: topDownOutputColumns * topDownOutputCellWidth,
        height: topDownOutputCellHeight,
        pixels: [UInt8](
            repeating: 0,
            count: topDownOutputColumns * topDownOutputCellWidth *
                topDownOutputCellHeight * 4
        )
    )
    for coat in 0..<coatCount {
        copyCell(
            recolored(fittedTopDown, coat: coat),
            index: coat,
            columns: topDownOutputColumns,
            output: &topDownOutput
        )
    }
    try writePNG(topDownOutput, path: topDownOutputPath)

    print(
        "wrote side atlas \(sideOutput.width)x\(sideOutput.height), " +
            "\(runFrameCount) run + \(jumpFrameCount) jump frames x \(coatCount) coats"
    )
    print(
        "wrote top-down atlas \(topDownOutput.width)x\(topDownOutput.height), " +
            "\(coatCount) coats"
    )
}

do {
    guard CommandLine.arguments.count == 6 else {
        throw BuildError.usage
    }
    try build(
        runPath: CommandLine.arguments[1],
        jumpPath: CommandLine.arguments[2],
        topDownPath: CommandLine.arguments[3],
        sideOutputPath: CommandLine.arguments[4],
        topDownOutputPath: CommandLine.arguments[5]
    )
} catch {
    FileHandle.standardError.write(Data("error: \(error)\n".utf8))
    exit(1)
}
