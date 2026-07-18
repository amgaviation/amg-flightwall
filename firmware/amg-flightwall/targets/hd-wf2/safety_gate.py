from SCons.Script import COMMAND_LINE_TARGETS, Exit

blocked_targets = {"erase", "program", "upload", "uploadfs"}
requested_targets = blocked_targets.intersection(COMMAND_LINE_TARGETS)

if requested_targets:
    print(
        "AMG safety gate: device writes are disabled until restoration is "
        "validated on a dedicated development controller."
    )
    Exit(2)
