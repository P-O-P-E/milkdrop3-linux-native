# Native fluid mode

Open the preset browser with Tab and enable **Native fluid mode** under Playback.
Choose Amber, Violet, Ocean, or Molten glass. The look selector, Next/Previous,
automatic preset duration, preset lock, and fade-duration slider work in this mode.
Selecting a library preset returns to projectM. The mode choice is session-only.

The native OpenGL renderer compiles one shader at startup and performs a warm-up
render before playback. Look changes interpolate parameters without reading files,
compiling shaders, loading textures, or saving play counts. Audio comes from the
same microphone, monitor, or macOS system-audio source as projectM, with time-based
amplitude smoothing. Linux and macOS use identical shader code.

The visuals are procedural metaballs with palette and shape changes, not a physical
fluid simulation. The fade slider controls the settling time of parameter blends;
there is no black midpoint within native mode. Switching between renderer modes is
immediate. Ordinary MilkDrop presets retain projectM's loading/compilation costs.

This mode avoids preset-change work but cannot guarantee zero driver/OS stalls.
It still renders at drawable resolution; GPU memory budgeting and dynamic resolution
are future improvements. Startup warm-up can increase launch time.
