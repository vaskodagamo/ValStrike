// INT-ART-05 evidence helper: record one process's own audio output (no microphone).
//
//   swift Art/Integration/KnifePilot01/record_app_audio.swift <pid> <seconds> <out.m4a>
//
// Uses ScreenCaptureKit app-audio capture (macOS 13+; needs the Screen Recording
// permission that `screencapture` already uses). Prints RECORDING_START and
// FIRST_AUDIO with Unix wall times so the audio can be aligned with client log cues.
import AVFoundation
import CoreMedia
import Foundation
import ScreenCaptureKit

let arguments = CommandLine.arguments
guard arguments.count == 4, let pid = pid_t(arguments[1]), let seconds = Double(arguments[2]) else {
    print("usage: record_app_audio.swift <pid> <seconds> <out.m4a>")
    exit(64)
}
let outputURL = URL(fileURLWithPath: arguments[3])

final class Recorder: NSObject, SCStreamOutput, SCStreamDelegate {
    let writer: AVAssetWriter
    let input: AVAssetWriterInput
    var started = false
    var buffers = 0

    init(url: URL) throws {
        try? FileManager.default.removeItem(at: url)
        writer = try AVAssetWriter(outputURL: url, fileType: .m4a)
        input = AVAssetWriterInput(mediaType: .audio, outputSettings: [
            AVFormatIDKey: kAudioFormatMPEG4AAC, AVSampleRateKey: 48000, AVNumberOfChannelsKey: 2, AVEncoderBitRateKey: 192000])
        input.expectsMediaDataInRealTime = true
        writer.add(input)
    }

    func stream(_ stream: SCStream, didOutputSampleBuffer sampleBuffer: CMSampleBuffer, of type: SCStreamOutputType) {
        guard type == .audio, sampleBuffer.isValid else { return }
        if !started {
            writer.startWriting()
            writer.startSession(atSourceTime: sampleBuffer.presentationTimeStamp)
            started = true
            print(String(format: "FIRST_AUDIO %.3f", Date().timeIntervalSince1970))
        }
        if input.isReadyForMoreMediaData {
            input.append(sampleBuffer)
            buffers += 1
        }
    }

    func stream(_ stream: SCStream, didStopWithError error: Error) {
        print("STOPPED \(error)")
    }
}

Task {
    do {
        let content = try await SCShareableContent.excludingDesktopWindows(false, onScreenWindowsOnly: false)
        guard let app = content.applications.first(where: { $0.processID == pid }), let display = content.displays.first else {
            print("NO_APP \(pid)")
            exit(2)
        }
        let filter = SCContentFilter(display: display, including: [app], exceptingWindows: [])
        let config = SCStreamConfiguration()
        config.capturesAudio = true
        config.sampleRate = 48000
        config.channelCount = 2
        config.width = 2
        config.height = 2
        config.minimumFrameInterval = CMTime(value: 1, timescale: 2)
        let recorder = try Recorder(url: outputURL)
        let stream = SCStream(filter: filter, configuration: config, delegate: recorder)
        try stream.addStreamOutput(recorder, type: .audio, sampleHandlerQueue: DispatchQueue(label: "audio"))
        try await stream.startCapture()
        print(String(format: "RECORDING_START %.3f", Date().timeIntervalSince1970))
        try await Task.sleep(nanoseconds: UInt64(seconds * 1_000_000_000))
        try await stream.stopCapture()
        if recorder.started {
            recorder.input.markAsFinished()
            await recorder.writer.finishWriting()
        }
        print("RECORDING_DONE buffers=\(recorder.buffers) status=\(recorder.writer.status.rawValue)")
        exit(0)
    } catch {
        print("ERROR \(error)")
        exit(1)
    }
}
dispatchMain()
