# Independent follow-up checks

Delivery: `f96ac2354f255b0d830b1219e316a14b458e6ea2`. Local macOS arm64,
Qt 6.11.1; the existing build is Debug/WERROR/UI_TESTS. Commands in each log
are rerunnable from that delivery's root. Paths are sanitized; output, time,
PID, warnings and verdicts are retained. Brain made no product/test edits.

`build.txt` and `ctest.txt` retain the build and offscreen CTest output/exits.
Native rows were separate sequential subprocesses with `QT_QPA_PLATFORM=cocoa`.
The four size/direction logs and unavailable log use passive diagnostic tracing.
The two `untraced-*` row logs unset `EDOPRO_NAVIGATION_TRACE` and keep the
uninterrupted-activation latch. Each native log retains its actual exit.

The foreground monitor uses the Builder's committed
`../27-native-filter-navigation-followup-evidence/foreground.swift`:

```sh
swiftc docs/batches/27-native-filter-navigation-followup-evidence/foreground.swift -o /tmp/edopro-next-foreground
/tmp/edopro-next-foreground > /tmp/edopro-next-foreground.txt &
monitor_pid=$!
# Run the preselected native rows sequentially; no competing Cocoa shell.
kill "$monitor_pid"
wait "$monitor_pid"
```

The monitor is intentionally terminated after the matrix; no test verdict
depends on its termination status. `foreground.txt` covers the traced matrix,
`untraced-foreground.txt` the two untraced rows. It samples foreground changes
every 5 ms; no claim of complete OS event history is made. Native teardown and
inter-row transitions are outside the activation session. No external peer
was deliberately injected by Brain in these runs; Builder's retained causal
intervention and product mutations were inspected separately.
