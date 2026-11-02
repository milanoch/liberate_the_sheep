// Historical v2 import: new run poses + old grab/jump artwork.
// After baking this intermediate atlas, apply pack_shepherd_dog_actions.swift
// with the v3 action source before installing the runtime PNG. See the art notes.
// Crops, registers and uniformly scales generated source images; no drawing,
// recolouring, skeleton deformation or animation preview is performed here.
// swift -module-cache-path /tmp/shepherd-dog-swift-cache \
//   SourceArt/Characters/pack_shepherd_dog_v2.swift
import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers

let sourceDirectory = "SourceArt/Characters/"
let runSourcePath = sourceDirectory + "shepherd_dog_run_v2_source.png"
let acceptedPath = sourceDirectory + "shepherd_dog_rescue_v1_accepted.png"
let keyframesPath = sourceDirectory + "shepherd_dog_rescue_v2_keyframes.png"
let jumpPath = sourceDirectory + "shepherd_dog_jump_v1_accepted.png"
let cell = 256

func load(_ path: String) -> CGImage {
    guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil),
          let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
        fatalError("Cannot read source art: \(path)")
    }
    return image
}

func makeContext(width: Int, height: Int, colorSpace: CGColorSpace) -> CGContext {
    guard let context = CGContext(data: nil, width: width, height: height,
        bitsPerComponent: 8, bytesPerRow: width * 4, space: colorSpace,
        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
        fatalError("Cannot allocate atlas")
    }
    context.clear(CGRect(x: 0, y: 0, width: width, height: height))
    context.interpolationQuality = .high
    context.setBlendMode(.copy)
    return context
}

func save(_ context: CGContext, to path: String) {
    guard let image = context.makeImage(),
          let destination = CGImageDestinationCreateWithURL(
              URL(fileURLWithPath: path) as CFURL,
              UTType.png.identifier as CFString, 1, nil) else {
        fatalError("Cannot write \(path)")
    }
    CGImageDestinationAddImage(destination, image, nil)
    guard CGImageDestinationFinalize(destination) else { fatalError("Cannot save \(path)") }
}

let runSource = load(runSourcePath)
let accepted = load(acceptedPath)
guard runSource.width == 1254 && runSource.height == 1254,
      accepted.width == 948 && accepted.height == 1659 else {
    fatalError("Source dimensions changed; update import registration before packing")
}
let colorSpace = accepted.colorSpace ?? CGColorSpace(name: CGColorSpace.sRGB)!
let width = cell * 4
let height = cell * 7
let atlas = makeContext(width: width, height: height, colorSpace: colorSpace)

// Split the 1254 px source into its four actual integer pixel regions. A single
// uniform scale preserves the natural leg and shoulder motion of every pose.
let cuts = [0, 314, 627, 941, 1254]
let scale = 0.67
// Anchors measured from each pose's muzzle axis, never its moving tail/paws.
// Registering the torso relative to this stable head axis prevents lateral
// sheet drift. A muzzle at Y=36 is .71875 board cells ahead for a 2-cell quad,
// matching the motion planner's .72-cell grip point.
let noseX: [Double] = [
    156.36,156.06,156.36,156.45,156.79,156.20,156.44,156.31,
    156.81,155.86,156.60,156.28,156.56,156.34,156.54,156.43,
]
let noseY: [Double] = [10,9,8,8,2,2,2,2,3,4,2,2,1,1,2,3]
for frame in 0..<16 {
    let column = frame % 4
    let row = frame / 4
    let sourceWidth = cuts[column + 1] - cuts[column]
    let sourceHeight = cuts[row + 1] - cuts[row]
    let rect = CGRect(x: cuts[column], y: cuts[row], width: sourceWidth, height: sourceHeight)
    guard let crop = runSource.cropping(to: rect) else { fatalError("Cannot crop run pose \(frame)") }
    let drawWidth = Double(sourceWidth) * scale
    let drawHeight = Double(sourceHeight) * scale
    let left = Double(column * cell + cell / 2) - noseX[frame] * scale
    let top = Double(row * cell) + 36.0 - noseY[frame] * scale
    atlas.saveGState()
    atlas.clip(to: CGRect(x: column * cell, y: height - (row + 1) * cell, width: cell, height: cell))
    atlas.draw(crop, in: CGRect(x: left, y: Double(height) - top - drawHeight,
                               width: drawWidth, height: drawHeight))
    atlas.restoreGState()
}

// Preserve the accepted grab sequence exactly in pose order. Its uniformly
// scaled cells keep the same normalized pivot and attachment coordinates.
let oldCell = 237
func acceptedPose(_ frame: Int) -> CGImage {
    let rect = CGRect(x: (frame % 4) * oldCell, y: (frame / 4) * oldCell,
                      width: oldCell, height: oldCell)
    guard let crop = accepted.cropping(to: rect) else { fatalError("Cannot crop accepted pose") }
    return crop
}
for frame in 16..<28 {
    atlas.draw(acceptedPose(frame), in: CGRect(
        x: (frame % 4) * cell, y: height - (frame / 4 + 1) * cell,
        width: cell, height: cell))
}
save(atlas, to: keyframesPath)

// The motion helper used old logical run pose 4 while airborne. Preserve it as
// a separate source cell so the denser runtime atlas can keep that same jump.
let jump = makeContext(width: cell, height: cell, colorSpace: colorSpace)
jump.draw(acceptedPose(4), in: CGRect(x: 0, y: 0, width: cell, height: cell))
save(jump, to: jumpPath)
print("Imported registered keyframes: \(keyframesPath)")
print("Preserved accepted airborne pose: \(jumpPath)")
