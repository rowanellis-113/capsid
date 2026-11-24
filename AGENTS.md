# Project development principles

1. Remove obsolete interfaces and behavior outright. Do not add compatibility layers, migration scripts or fallback paths.
2. Choose the simplest implementation that meets the current requirements. Add abstractions and configuration only when a concrete requirement calls for them.
3. Build the system in layers. Get the smallest end-to-end implementation working first, then extend it. Keep working components in place until an actual requirement justifies replacing them.
4. Keep components modular and give each one a clear responsibility.
5. Prefer mature, actively maintained third-party libraries. Reimplement their functionality only for a compelling reason.
6. Check what the project's existing dependencies can do before adding a package or writing custom code. Verify their capabilities directly.
7. Make architectural decisions that will hold up over time. Reject temporary designs that defer a known problem.
8. Study how established products solve the same problem and use patterns that have proved effective in practice.
