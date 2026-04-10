#General

This cheat is an external utility for Counter-Strike 2 that provides a wide range of visual and gameplay advantages while employing several anti‑detection techniques. It runs as a separate process and reads or writes CS2 memory using low‑level system calls (NtReadVirtualMemory / NtWriteVirtualMemory) instead of standard Windows API functions, helping to bypass user‑mode hooks. To further avoid detection, it never calls OpenProcess directly; instead, it enumerates system handles to find a process that already has a handle to the game (such as svchost.exe or lsass.exe) and duplicates that handle, gaining read, write, and operation access. Critical API calls are wrapped in call‑stack spoofing macros that temporarily overwrite the return address on the stack, breaking stack walking by anti‑cheat software. Additionally, all sensitive strings are obfuscated using XOR encryption at compile time.

The cheat renders its interface with ImGui over DirectX 11. Instead of creating its own overlay window, it first tries to hijack an existing overlay like the NVIDIA GeForce Overlay or Discord window, modifying its styles to become layered and click‑through when the menu is hidden, and subclassing its window procedure to route input to ImGui. If no suitable overlay is found, it falls back to a custom transparent, top‑most window. The menu itself is toggled with the INSERT key and features a tactical amber‑themed design with tabs for visuals, aimbot, and miscellaneous features. All ESP colors, aimbot settings, triggerbot delay, and keybinds can be adjusted in real time.

The ESP system draws bounding boxes, health bars, skeleton bones, player names, snaplines, armor bars, and distance indicators for all valid enemies, with optional team checking and glow effects. The glow can be health‑based, team‑based, or a static color. The aimbot activates on a configurable hotkey (default right mouse button) and locks onto selected bone targets (head, neck, chest, pelvis) within a defined field of view, with smoothing and a visibility check that uses the game’s own spotted state mask. Recoil control subtracts the current aim punch from the final angle. The triggerbot automatically fires when an enemy is under the crosshair while the hotkey is held, with an adjustable delay. Bunnyhop repeatedly sends spacebar messages to the game window when the player is on the ground, and the no‑flash feature nullifies flashbang effects by writing zero to the flash alpha value.

All game data – entity positions, health, teams, bone arrays, view matrix, and local player information – is read once per frame using cached offsets defined in a separate header. The cheat updates these offsets after each game update to remain functional. Overall, this cheat is a fully featured external tool that prioritises stealth through handle hijacking, syscalls, stack spoofing, and overlay hijacking, while offering a complete set of ESP, aimbot, triggerbot, and movement assistance features.





#Feature list:

ESP:

Element	Description
Box	2D bounding box around the enemy (outline + optional fill).
Skeleton	Bone-based stick figure (head, spine, limbs).
Health Bar	Vertical bar on the left side of the box, with numeric value.
Name	Player name displayed above the box.
Snapline	Line from enemy’s feet to the bottom center of the screen.
Armor Bar	Thin horizontal bar below the box (kevlar value).
Distance	Distance in meters shown below the box.
Glow	Outline glow around enemy models (supports health‑based, team‑based, or static color).
All colors are fully customizable via the menu.

Aimbot:

Activation: Configurable hotkey (default: right mouse button).
Field of View (FOV): Circular FOV limit (degrees) with optional on‑screen circle overlay.
Smoothing: Adjustable from 1 (instant) to 20 (very slow).
Target Bones: Head, neck, chest, pelvis – multiple can be selected (priority by lowest FOV).
Visibility Check: Only shoot at enemies that are not occluded (uses m_entitySpottedState mask). (CURRENTLY FAILS!)
RCS (Recoil Control System): Compensates for aim punch by subtracting m_aimPunchAngle * 2 from the final angle. (CURRENTLY FAILS!)
FOV Circle Overlay: Draws the aimbot’s effective FOV on screen (color configurable).

Triggerbot

Activation: Configurable hotkey (default: ALT).
Delay: Adjustable milliseconds between the target appearing under crosshair and the shot being fired.
Behavior: Simulates a mouse down event while the hotkey is held and an enemy is in the crosshair (using m_iIDEntIndex). Automatically releases the mouse when the hotkey is released or target is lost.

Miscellaneous
Bunnyhop: Automatically re‑presses the spacebar when the player is on the ground (m_fFlags & FL_ONGROUND). Uses SendMessage to the game window to avoid direct key state manipulation.
No Flash: Sets m_flFlashMaxAlpha to 0, completely nullifying flashbang effects.
