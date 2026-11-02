// Imports the generated, unevenly spaced source poses into Panda's uniform atlas.
// This only crops, translates and pads existing art; it does not redraw poses.
// Run from the project root:
// swift -module-cache-path /tmp/shepherd-dog-swift-cache \
//   SourceArt/Characters/pack_shepherd_dog.swift
import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers

let sourcePath = "SourceArt/Characters/shepherd_dog_rescue_source.png"
// Intermediate only: the runtime atlas also has a final imagegen edge cleanup.
// Never overwrite that cleaned asset by rerunning the packing stage.
let destinationPath = "SourceArt/Characters/shepherd_dog_rescue_packed_intermediate.png"
let sourceURL = URL(fileURLWithPath: sourcePath)
guard let source = CGImageSourceCreateWithURL(sourceURL as CFURL, nil),
      let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
    fatalError("Cannot read dog source atlas: \(sourcePath)")
}

// The source contains four columns but its seven rows are not uniformly spaced.
// Split only at transparent gaps so no tail is carried into its neighbour.
let rowEdges = [0, 252, 489, 730, 970, 1210, 1431, 1659]
let sourceColumnWidth = 237
let cell = 256
let width = 4 * cell
let height = 7 * cell
// Nose axis and shoulder collar + 35 px define a stable torso anchor. The tail
// is deliberately excluded, so its wag never moves the entire character.
let pivots: [(Int, Int)] = [
    (145,149), (129,145), (112,145), (97,149),
    (142,117), (128,112), (110,113), (97,113),
    (142,112), (131,112), (114,116), (99,112),
    (141,113), (128,116), (112,116), (98,112),
    (139,118), (127,115), (112,118), (98,118),
    (140,116), (128,111), (111,116), (96,115),
    (140,106), (127,110), (113,107), (98,107),
]
let colorSpace = image.colorSpace ?? CGColorSpace(name: CGColorSpace.sRGB)!
guard let context = CGContext(data: nil, width: width, height: height,
    bitsPerComponent: 8, bytesPerRow: width * 4, space: colorSpace,
    bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
    fatalError("Cannot create dog atlas")
}
context.clear(CGRect(x: 0, y: 0, width: width, height: height))
context.interpolationQuality = .none
context.setShouldAntialias(false)
context.setBlendMode(.copy)
for frame in 0..<28 {
    let column = frame % 4
    let row = frame / 4
    let sourceHeight = rowEdges[row + 1] - rowEdges[row]
    let cropRect = CGRect(x: column * sourceColumnWidth, y: rowEdges[row],
                          width: sourceColumnWidth, height: sourceHeight)
    guard let crop = image.cropping(to: cropRect) else {
        fatalError("Cannot crop dog pose \(frame)")
    }
    let x = column * cell + cell / 2 - pivots[frame].0
    let top = row * cell + cell / 2 - pivots[frame].1
    // CoreGraphics draw coordinates grow upward; source crop coordinates grow
    // downward. Integer placement preserves the original pixel-sized detail.
    context.saveGState()
    context.clip(to: CGRect(x: column * cell, y: height - (row + 1) * cell,
                           width: cell, height: cell))
    context.draw(crop, in: CGRect(x: x, y: height - top - sourceHeight,
                                 width: sourceColumnWidth, height: sourceHeight))
    context.restoreGState()
}
guard let packed = context.makeImage(),
      let destination = CGImageDestinationCreateWithURL(
          URL(fileURLWithPath: destinationPath) as CFURL,
          UTType.png.identifier as CFString, 1, nil) else {
    fatalError("Cannot write dog atlas")
}
CGImageDestinationAddImage(destination, packed, nil)
guard CGImageDestinationFinalize(destination) else { fatalError("Cannot save dog atlas") }
print("Packed dog poses into \(destinationPath) (\(width)×\(height), 4×7 cells)")
