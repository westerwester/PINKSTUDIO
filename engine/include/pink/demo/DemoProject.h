// PINKSTUDIO - built-in demo session "Pink Sugar Rush" (150 BPM, 8 bars, D minor).
// Used by the first build to prove the whole chain: MIDI clips -> instruments -> mixer
// inserts/sends/sidechain -> master limiter -> metering -> device / WAV export.
#pragma once

#include "pink/model/Project.h"

namespace pink
{
/** Replaces the project content with the demo session. */
void buildDemoProject (Project& project);

} // namespace pink
