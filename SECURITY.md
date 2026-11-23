# Security

This is a small compatibility project with limited hardware validation, no support SLA, no signing infrastructure, and no claim of a security audit. Use a trusted original DLL: forwarding intentionally executes it. Absolute sibling loading restricts search paths but does not authenticate that DLL.

Do not report issues with original/game binaries, full memory dumps, card data, raw machine logs, serials, credentials or personal paths attached. Redact first. For a reproducible bridge problem, share minimal source/config examples and build/version information only.

No private reporting address has been configured in this publication draft. Before public release, the maintainer should enable GitHub private vulnerability reporting (repository Settings → Security) or publish an appropriate private contact. Do not post a sensitive exploit or private data to a public issue merely because a private channel is not yet available.

Supported scope is the current source snapshot on Windows x64 and the documented controller protocol; upstream IO bugs are not automatically repaired by this bridge. Kernel/driver cancellation that never completes may stall the writer; exit the process before attempting recovery.
