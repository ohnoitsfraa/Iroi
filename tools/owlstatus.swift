// Asks an OWL device for its settings (CC 67 = 127) and prints the sysex
// replies: preset names, program messages, stats and errors.
//   owlstatus DEVICE [seconds]
import CoreMIDI
import Foundation

let args = CommandLine.arguments
let name = args.count > 1 ? args[1] : "OWL"
let seconds = args.count > 2 ? Double(args[2])! : 2

func endpointName(_ ep: MIDIEndpointRef) -> String {
    var n: Unmanaged<CFString>?
    MIDIObjectGetStringProperty(ep, kMIDIPropertyName, &n)
    return (n?.takeRetainedValue() as String?) ?? ""
}
var dest: MIDIEndpointRef = 0, src: MIDIEndpointRef = 0
for i in 0..<MIDIGetNumberOfDestinations() where endpointName(MIDIGetDestination(i)).contains(name) { dest = MIDIGetDestination(i) }
for i in 0..<MIDIGetNumberOfSources() where endpointName(MIDIGetSource(i)).contains(name) { src = MIDIGetSource(i) }
guard dest != 0, src != 0 else { print("no MIDI device matching \(name)"); exit(1) }

let commands: [UInt8: String] = [0x01: "preset", 0x02: "parameter", 0x03: "config", 0x04: "resource",
    0x20: "firmware", 0x21: "device id", 0x22: "message", 0x23: "device stats", 0x24: "program stats",
    0x25: "bootloader", 0x30: "ERROR"]
var buffer: [UInt8] = []
let lock = NSLock()

var client = MIDIClientRef()
MIDIClientCreateWithBlock("owlstatus" as CFString, &client, nil)
var inPort = MIDIPortRef()
MIDIInputPortCreateWithBlock(client, "in" as CFString, &inPort) { list, _ in
    var p = list.pointee.packet
    for _ in 0..<list.pointee.numPackets {
        let bytes = withUnsafeBytes(of: p.data) { Array($0.prefix(Int(p.length))) }
        lock.lock(); buffer += bytes; lock.unlock()
        p = MIDIPacketNext(&p).pointee
    }
}
MIDIPortConnectSource(inPort, src, nil)
var outPort = MIDIPortRef()
MIDIOutputPortCreate(client, "out" as CFString, &outPort)

var packets = MIDIPacketList()
let cc: [UInt8] = [0xB0, 67, 127]
let pkt = MIDIPacketListInit(&packets)
MIDIPacketListAdd(&packets, MemoryLayout<MIDIPacketList>.size, pkt, 0, cc.count, cc)
MIDISend(outPort, dest, &packets)

RunLoop.current.run(until: Date().addingTimeInterval(seconds))

lock.lock(); let all = buffer; lock.unlock()
var start = -1
for (i, b) in all.enumerated() {
    if b == 0xF0 { start = i }
    if b == 0xF7 && start >= 0 {
        let msg = Array(all[start...i]); start = -1
        guard msg.count > 4 else { continue }
        let cmd = msg[3], body = msg[4..<(msg.count - 1)]
        let text = String(bytes: body.filter { $0 >= 0x20 && $0 < 0x7F }, encoding: .ascii) ?? ""
        print("\(commands[cmd] ?? String(format: "0x%02x", cmd)): \(text)")
    }
}
