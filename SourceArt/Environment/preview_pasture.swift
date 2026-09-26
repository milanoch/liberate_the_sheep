// Offline composition of actual assets, not a game/editor launch or new artwork.
// swiftc -O SourceArt/Environment/preview_pasture.swift -o /tmp/preview_pasture
// Usage: preview_pasture INPUT.json PROJECT_ROOT OUTPUT_DIRECTORY
import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

struct Sprite: Decodable {
    let kind: String
    let x: Double
    let y: Double
    let width: Double
    let height: Double
    let rotation: Double
    let opacity: Double
    let index: Int
}
struct Preview: Decodable {
    let level: Int
    let width: Int
    let height: Int
    let ortho: Double
    let cellSize: Double
    let groundFile: String
    let groundWidth: Int
    let groundHeight: Int
    let sprites: [Sprite]
    let sheepCount: Int
}
enum PreviewError: Error { case failed(String) }

func loadImage(_ path: String) throws -> CGImage {
    guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil),
          let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
        throw PreviewError.failed("Cannot decode \(path)")
    }
    return image
}

func loadGround(_ preview: Preview, relativeTo directory: URL) throws -> CGImage {
    let data = try Data(contentsOf: directory.appendingPathComponent(preview.groundFile))
    guard preview.groundWidth > 0 && preview.groundHeight > 0,
          data.count == preview.groundWidth * preview.groundHeight * 4,
          let provider = CGDataProvider(data: data as CFData),
          let image = CGImage(width: preview.groundWidth, height: preview.groundHeight,
              bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: preview.groundWidth * 4,
              space: CGColorSpace(name: CGColorSpace.sRGB)!,
              bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue |
                  CGBitmapInfo.byteOrder32Big.rawValue), provider: provider,
              decode: nil, shouldInterpolate: true, intent: .defaultIntent) else {
        throw PreviewError.failed("Invalid composed ground image \(preview.groundFile)")
    }
    return image
}
func run() throws {
    guard CommandLine.arguments.count == 4 else {
        throw PreviewError.failed("Usage: preview_pasture INPUT.json PROJECT_ROOT OUTPUT_DIRECTORY")
    }
    let input = URL(fileURLWithPath: CommandLine.arguments[1])
    let previews = try JSONDecoder().decode([Preview].self, from: Data(contentsOf: input))
    let root = CommandLine.arguments[2]
    let outputDirectory = URL(fileURLWithPath: CommandLine.arguments[3], isDirectory: true)
    try FileManager.default.createDirectory(at: outputDirectory, withIntermediateDirectories: true)
    let sheepAtlas = try loadImage(root + "/Assets/Textures/Characters/sheep_topdown_run.png")
    let fence = try loadImage(root + "/Assets/Textures/Environment/wood_fence_panel.png")
    guard sheepAtlas.width == 6400 && sheepAtlas.height == 6144 else {
        throw PreviewError.failed("Unexpected atlas layout")
    }
    var sheep: [Int: CGImage] = [:]
    for encoded in [0, 100, 200] {
        // The imported Panda sprite index uses modulo for both coordinates.
        sheep[encoded] = sheepAtlas.cropping(to: CGRect(x: (encoded % 25) * 256,
            y: (encoded % 12) * 512, width: 256, height: 512))!
    }
    for preview in previews {
        let space = CGColorSpace(name: CGColorSpace.sRGB)!
        let bitmap = CGImageAlphaInfo.premultipliedLast.rawValue | CGBitmapInfo.byteOrder32Big.rawValue
        guard let context = CGContext(data: nil, width: preview.width, height: preview.height,
            bitsPerComponent: 8, bytesPerRow: preview.width * 4, space: space, bitmapInfo: bitmap) else {
            throw PreviewError.failed("Cannot allocate preview")
        }
        context.interpolationQuality = .high
        // This is the game's one opaque full-viewport surface, not a reenactment
        // using overlapping grass tiles or transparent wear decals.
        let ground = try loadGround(preview, relativeTo: input.deletingLastPathComponent())
        context.draw(ground, in: CGRect(x: 0, y: 0, width: preview.width, height: preview.height))
        let pixelsPerWorld = Double(preview.height) / preview.ortho
        for sprite in preview.sprites {
            let image: CGImage
            switch sprite.kind {
            case "fence": image = fence
            case "sheep": image = sheep[sprite.index]!
            default: throw PreviewError.failed("Unknown sprite kind \(sprite.kind)")
            }
            context.saveGState()
            context.setAlpha(sprite.opacity)
            context.translateBy(x: Double(preview.width) * 0.5 + sprite.x * pixelsPerWorld,
                                y: Double(preview.height) * 0.5 + sprite.y * pixelsPerWorld)
            context.rotate(by: sprite.rotation * .pi / 180)
            context.scaleBy(x: sprite.width < 0 ? -1 : 1, y: sprite.height < 0 ? -1 : 1)
            let w = abs(sprite.width) * pixelsPerWorld
            let h = abs(sprite.height) * pixelsPerWorld
            context.draw(image, in: CGRect(x: -w * 0.5, y: -h * 0.5, width: w, height: h))
            context.restoreGState()
        }
        let output = outputDirectory.appendingPathComponent(
            "pasture-level-\(preview.level)-\(preview.width)x\(preview.height).png")
        guard let image = context.makeImage(), let destination = CGImageDestinationCreateWithURL(
            output as CFURL, UTType.png.identifier as CFString, 1, nil) else {
            throw PreviewError.failed("Cannot create output PNG")
        }
        CGImageDestinationAddImage(destination, image, nil)
        guard CGImageDestinationFinalize(destination) else {
            throw PreviewError.failed("Cannot save output PNG")
        }
        print("\(output.path): \(preview.sheepCount) sheep, single \(preview.groundWidth)×\(preview.groundHeight) opaque ground, cell \(preview.cellSize * pixelsPerWorld) px")
    }
}
do { try run() }
catch {
    FileHandle.standardError.write(Data("\(error)\n".utf8))
    exit(1)
}
