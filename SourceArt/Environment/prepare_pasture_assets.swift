#!/usr/bin/env swift
// Reproducible transparent-margin packaging for the generated fence.
// Default: lossless crop. Optional canvas: uniform aspect-fit with transparent
// padding, never nonuniformly stretch the artwork or replace its alpha.
// Optional --bleed-transparent-rgb: retain alpha, pad invisible RGB for Panda's
// alpha-tested Sprite pass so linear/mip filtering cannot sample black margins.
import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers

let bleedTransparentRGB = CommandLine.arguments.contains("--bleed-transparent-rgb")
let arguments = CommandLine.arguments.filter { $0 != "--bleed-transparent-rgb" }
guard arguments.count == 3 || arguments.count == 5 else {
    fatalError("Usage: swift prepare_pasture_assets.swift FENCE_SOURCE.png FENCE_OUTPUT.png [CANVAS_WIDTH CANVAS_HEIGHT] [--bleed-transparent-rgb]")
}
let input = URL(fileURLWithPath: arguments[1])
let output = URL(fileURLWithPath: arguments[2])
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
guard let crop = image.cropping(to: CGRect(x: x, y: y, width: cropWidth, height: cropHeight)) else {
    fatalError("Cannot create cropped fence")
}
var packed = crop
if arguments.count == 5 {
    guard let canvasWidth = Int(arguments[3]),
          let canvasHeight = Int(arguments[4]),
          canvasWidth > 0, canvasHeight > 0,
          canvasWidth <= 8192, canvasHeight <= 8192,
          let context = CGContext(data: nil, width: canvasWidth, height: canvasHeight,
              bitsPerComponent: 8, bytesPerRow: canvasWidth * 4,
              space: CGColorSpace(name: CGColorSpace.sRGB)!,
              bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
        fatalError("Invalid canvas dimensions or cannot allocate transparent canvas")
    }
    let scale = min(CGFloat(canvasWidth) / CGFloat(cropWidth),
                    CGFloat(canvasHeight) / CGFloat(cropHeight))
    let drawWidth = CGFloat(cropWidth) * scale
    let drawHeight = CGFloat(cropHeight) * scale
    context.clear(CGRect(x: 0, y: 0, width: canvasWidth, height: canvasHeight))
    context.interpolationQuality = .high
    context.draw(crop, in: CGRect(x: (CGFloat(canvasWidth) - drawWidth) * 0.5,
                                 y: (CGFloat(canvasHeight) - drawHeight) * 0.5,
                                 width: drawWidth, height: drawHeight))
    guard let fitted = context.makeImage() else { fatalError("Cannot create fitted fence") }
    packed = fitted
    print("Uniform aspect-fit scale \(scale), transparent canvas \(canvasWidth)×\(canvasHeight)")
}
if bleedTransparentRGB {
    let pixelCount = packed.width * packed.height
    let packedWidth = packed.width
    var rgba = [UInt8](repeating: 0, count: pixelCount * 4)
    let colorSpace = CGColorSpace(name: CGColorSpace.sRGB)!
    rgba.withUnsafeMutableBytes { bytes in
        let context = CGContext(data: bytes.baseAddress, width: packed.width,
            height: packed.height, bitsPerComponent: 8, bytesPerRow: packedWidth * 4,
            space: colorSpace, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
        context.draw(packed, in: CGRect(x: 0, y: 0, width: packed.width, height: packed.height))
    }

    // CoreGraphics draws premultiplied RGBA. Export straight RGB before padding:
    // the stock Sprite shader outputs sampled RGB, not RGB divided by alpha.
    var owners = [Int](repeating: -1, count: pixelCount)
    var queue: [Int] = []
    queue.reserveCapacity(pixelCount)
    for pixel in 0..<pixelCount {
        let offset = pixel * 4
        let alpha = Int(rgba[offset + 3])
        if alpha > 0 {
            for channel in 0..<3 {
                rgba[offset + channel] = UInt8(min(255,
                    (Int(rgba[offset + channel]) * 255 + alpha / 2) / alpha))
            }
        }
        // Borrow real wood colors, not noisy near-transparent generated pixels.
        if alpha >= 240 {
            owners[pixel] = pixel
            queue.append(pixel)
        }
    }
    guard !queue.isEmpty else { fatalError("Fence has no opaque wood for RGB padding") }

    // Multi-source flood fill extends edge colors throughout the empty margin,
    // including the distant texels used by smaller mip levels.
    var head = 0
    while head < queue.count {
        let pixel = queue[head]
        head += 1
        let column = pixel % packedWidth
        let neighbours = [column > 0 ? pixel - 1 : -1,
                          column + 1 < packedWidth ? pixel + 1 : -1,
                          pixel >= packedWidth ? pixel - packedWidth : -1,
                          pixel + packedWidth < pixelCount ? pixel + packedWidth : -1]
        for neighbour in neighbours where neighbour >= 0 && owners[neighbour] == -1 {
            owners[neighbour] = owners[pixel]
            queue.append(neighbour)
        }
    }
    var paddedCount = 0
    for pixel in 0..<pixelCount where rgba[pixel * 4 + 3] < 16 {
        let sourceOffset = owners[pixel] * 4
        for channel in 0..<3 {
            rgba[pixel * 4 + channel] = rgba[sourceOffset + channel]
        }
        // Alpha is never modified: no filled background or altered silhouette.
        paddedCount += 1
    }
    let data = Data(rgba)
    guard let provider = CGDataProvider(data: data as CFData),
          let padded = CGImage(width: packed.width, height: packed.height,
              bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: packedWidth * 4,
              space: colorSpace, bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.last.rawValue),
              provider: provider, decode: nil, shouldInterpolate: true,
              intent: .defaultIntent) else {
        fatalError("Cannot create straight-alpha RGB-padded fence")
    }
    packed = padded
    print("Padded RGB in \(paddedCount) transparent/near-transparent pixels; alpha retained")
}
guard let destination = CGImageDestinationCreateWithURL(output as CFURL,
          UTType.png.identifier as CFString, 1, nil) else {
    fatalError("Cannot create fence PNG destination")
}
CGImageDestinationAddImage(destination, packed, nil)
guard CGImageDestinationFinalize(destination) else { fatalError("Cannot write fence PNG") }
print("Fence source \(width)×\(height), alpha>8 bounds \(left),\(top)..\(right),\(bottom)")
print("Crop \(cropWidth)×\(cropHeight); crop visible bounds \(left-x),\(top-y)..\(right-x),\(bottom-y)")
print("Output \(packed.width)×\(packed.height)")
