# darof

Force external disks on macOS to mount read-only, or block them from mounting.

`darof` registers a Disk Arbitration mount approval callback
(`DARegisterDiskMountApprovalCallback`) and dissents on every mount request.

## Build

```
make
make install
```

Command Line Tools are enough. Xcode is not required.

## Usage

```
darof            # external disks mount read-only
darof --deny     # external disks do not mount
darof --all      # apply to all disks, not only external
```

## Run at login

```
cp pl.maurycy.darof.plist ~/Library/LaunchAgents/
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/pl.maurycy.darof.plist
```

Stop:

```
launchctl bootout gui/$(id -u)/pl.maurycy.darof
```

The agent starts again at next login. Remove the plist to disable it permanently.

Log: `~/Library/Logs/darof.log`.

## How it works

- Read-only mode returns the private dissenter status `0xF8DAFF02`. `diskarbitrationd` then mounts the volume with `nowrite`. This code is undocumented and can change.
- `--deny` returns `kDAReturnNotPermitted`.
- The external filter matches `DADeviceInternal = false`.

## Limitations

- **Not a security boundary.** `mount(8)` and `mount(2)` bypass Disk Arbitration.
- **Unresponsive process.** If `darof` does not answer within 10 seconds, the mount is approved.
- **Disk images.** Images attached with `hdiutil` or `diskutil image` on macOS 27 do not go through the approval callback. They also have no `DADeviceInternal` key.
- **Filesystem only.** Read-only applies to the filesystem, not to the block device. For forensics, use a hardware write blocker.
- **Time Machine.** A backup disk mounted read-only breaks backups.
- **`--all`.** It also affects internal APFS volumes. Do not run it permanently.

## Status

- **Verified** on macOS 27 (arm64): read-only and deny modes, using an image mounted through `DADiskMount`.
- **Not verified:** a physical USB disk.
