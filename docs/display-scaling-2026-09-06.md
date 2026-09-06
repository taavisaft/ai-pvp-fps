# Display scaling investigation — 2026-09-06

The renderer cached its startup drawable size and restored that viewport after every shadow pass. Moving a 1280×720 logical window from Retina (2560×1440 backing pixels) to a standard-DPI monitor left the viewport twice as wide and tall as the new framebuffer, cropping the picture.

Window and drawable sizes are now refreshed after SDL event polling. Positive logical dimensions update the aspect/HUD sizing; positive backing dimensions update the viewport and shadow-pass restoration size. Zero-sized transitional/minimized results retain the last valid dimensions. No window resizing or DPI override is forced.

The regression test simulates Retina → standard DPI → Retina, unchanged dimensions, resizing and zero-sized transitions with mocked window/GL calls. All seven packaged-build CTest suites passed. The Apple Silicon app/ZIP was rebuilt and ad-hoc signature verification passed. The extracted app launched from outside the repository and produced a frame successfully. The user subsequently confirmed that Retina-to-standard-DPI dragging still displays incorrectly. This change is incomplete: automated proof covers reported size changes but does not reproduce the remaining native display-transition problem. Investigation of SDL/Cocoa backing-surface updates is unfinished.

The local package was updated; the hosted GitHub release was not replaced.
