#!/usr/bin/env swift
// Reproducible, lossless transparent-margin packaging for the generated fence.
// Does not repaint, resize, recolor, or change the generated alpha channel.
import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers

guard CommandLine.arguments.count == 3 else {
    fatalError("Usage: swift prepare_pasture_assets.swift FENCE_SOURCE.png FENCE_OUTPUT.png")
}
let input = URL(fileURLWithPath: CommandLine.arguments[1])
let output = URL(fileURLWithPath: CommandLine.arguments[2])
guard let source = CGImageSourceCreateWithURL(input as CFURL, nil),
      let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
    fatalError("Cannot read fence source")
}
let width = image.width
let height = image.height
var pixels = [UInt8](repeating: 0, count: width * height * 4)
pixels.withUnsafeMutableBytes { bytes in
    let context = CGContext(data: bytes.baseAddress, width: width, height: height,
        bitsPerComponent: 8, bytesPerRow: width * 4,
        space: CGColorSpaceCreateDeviceRGB(),
        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
}
// Ignore imperceptible generated speckles solely when locating the margin.
// Source pixels and their original alpha within the crop remain untouched.
var left = width, right = -1, top = height, bottom = -1
for y in 0..<height {
    for x in 0..<width where pixels[(y * width + x) * 4 + 3] > 8 {
        left = min(left, x)
        right = max(right, x)
        top = min(top, y)
        bottom = max(bottom, y)
    }
}
guard right >= left else { fatalError("Fence image is empty") }
let padding = 4
let x = max(0, left - padding)
let y = max(0, top - padding)
let cropWidth = min(width - 1, right + padding) - x + 1
let cropHeight = min(height - 1, bottom + padding) - y + 1
guard let crop = image.cropping(to: CGRect(x: x, y: y, width: cropWidth, height: cropHeight)),
      let destination = CGImageDestinationCreateWithURL(output as CFURL,
          UTType.png.identifier as CFString, 1, nil) else {
    fatalError("Cannot create cropped fence")
}
CGImageDestinationAddImage(destination, crop, nil)
guard CGImageDestinationFinalize(destination) else { fatalError("Cannot write fence PNG") }
print("Fence source \(width)×\(height), alpha>8 bounds \(left),\(top)..\(right),\(bottom)")
print("Output \(cropWidth)×\(cropHeight); visible bounds \(left-x),\(top-y)..\(right-x),\(bottom-y)")
