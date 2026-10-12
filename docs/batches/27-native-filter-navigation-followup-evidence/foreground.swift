import AppKit
import Foundation
var last: pid_t = -1
let timer = Timer.scheduledTimer(withTimeInterval: 0.005, repeats: true) { _ in
    if let app = NSWorkspace.shared.frontmostApplication, app.processIdentifier != last {
        last = app.processIdentifier
        print("FOREGROUND", Date().timeIntervalSince1970, last, app.bundleIdentifier ?? "none", app.localizedName ?? "none")
        fflush(stdout)
    }
}
RunLoop.main.run()
