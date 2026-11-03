// Sends each F0..F7 message of a .syx file to a CoreMIDI destination whose
// name contains the given string, with a short delay between messages.
//   sendsyx FILE DEVICE [delay_ms]
import CoreMIDI
import Foundation

let args = CommandLine.arguments
guard args.count >= 3, let data = FileManager.default.contents(atPath: args[1]) else {
    print("usage: sendsyx FILE DEVICE [delay_ms]"); exit(1)
}
let delay = args.count > 3 ? UInt32(args[3])! : 2

var dest: MIDIEndpointRef = 0
for i in 0..<MIDIGetNumberOfDestinations() {
    let ep = MIDIGetDestination(i)
    var name: Unmanaged<CFString>?
    MIDIObjectGetStringProperty(ep, kMIDIPropertyName, &name)
    if let n = name?.takeRetainedValue() as String?, n.contains(args[2]) { dest = ep; break }
}
guard dest != 0 else { print("no MIDI destination matching \(args[2])"); exit(1) }

var client = MIDIClientRef()
var status = MIDIClientCreateWithBlock("sendsyx" as CFString, &client, nil)
guard status == noErr else { print("MIDIClientCreate failed: \(status)"); exit(1) }

let bytes = [UInt8](data)
var messages: [[UInt8]] = []
var start = -1
for (i, b) in bytes.enumerated() {
    if b == 0xF0 { start = i }
    if b == 0xF7 && start >= 0 { messages.append(Array(bytes[start...i])); start = -1 }
}

for (n, msg) in messages.enumerated() {
    let done = DispatchSemaphore(value: 0)
    msg.withUnsafeBufferPointer { buf in
        let req = UnsafeMutablePointer<MIDISysexSendRequest>.allocate(capacity: 1)
        let ctx = Unmanaged.passRetained(done as AnyObject).toOpaque()
        req.initialize(to: MIDISysexSendRequest(destination: dest, data: buf.baseAddress!,
            bytesToSend: UInt32(msg.count), complete: false, reserved: (0, 0, 0),
            completionProc: { r in
                let s = Unmanaged<AnyObject>.fromOpaque(r.pointee.completionRefCon!).takeRetainedValue()
                (s as! DispatchSemaphore).signal()
            }, completionRefCon: ctx))
        status = MIDISendSysex(req)
        if status != noErr { print("MIDISendSysex failed on message \(n): \(status)"); exit(1) }
        done.wait()
        req.deallocate()
    }
    usleep(delay * 1000)
}
print("sent \(messages.count) sysex messages (\(bytes.count) bytes)")
