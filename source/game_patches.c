/* game_patches.c -- compatibility fixes for the original Android modules
 *
 * The Android GameUI contains console-specific behavior that can conflict
 * with user settings on Switch. Keep these changes small and validate the
 * surrounding instructions before modifying a loaded module.
 */

#include <stdint.h>
#include <string.h>

#include "game_patches.h"
#include "so_util.h"
#include "util.h"

static int patch_save_dialog_fastswitch(so_module *gameui) {
  static const char symbol[] =
      "_ZN19CBaseSaveGameDialog16OnKeyCodePressedE12ButtonCode_t";

  // This is the body of:
  //   if (hud_fastswitch.IsValid() && hud_fastswitch.GetInt() != 2)
  //       hud_fastswitch.SetValue(2);
  //
  // The first instruction branches past the body only when the ConVar is
  // invalid. Replacing it with an unconditional branch preserves the user's
  // value when a save/load action is confirmed with controller A.
  static const uint32_t force_fastswitch[] = {
    0x36000140, // tbz w0, #0, +0x28
    0xf9400be8, // ldr x8, [sp, #16]
    0xb9405908, // ldr w8, [x8, #88]
    0x7100091f, // cmp w8, #2
    0x540000c0, // b.eq +0x18
    0xf94007e0, // ldr x0, [sp, #8]
    0x52800041, // mov w1, #2
    0xf9400008, // ldr x8, [x0]
    0xf9400908, // ldr x8, [x8, #16]
    0xd63f0100, // blr x8
  };
  static const uint32_t skip_fastswitch = 0x1400000a; // b +0x28

  size_t function_size = 0;
  uint32_t *function = (uint32_t *)so_find_addr_with_size(gameui, symbol, &function_size);
  if (!function || function_size < sizeof(force_fastswitch)) {
    debugPrintf("GameUI patch: save dialog symbol unavailable\n");
    return 0;
  }

  uint32_t *match = NULL;
  const size_t instruction_count = function_size / sizeof(uint32_t);
  const size_t pattern_count = sizeof(force_fastswitch) / sizeof(force_fastswitch[0]);

  for (size_t i = 0; i + pattern_count <= instruction_count; i++) {
    if (memcmp(function + i, force_fastswitch, sizeof(force_fastswitch)) != 0)
      continue;
    if (match) {
      debugPrintf("GameUI patch: ambiguous save dialog instruction pattern\n");
      return 0;
    }
    match = function + i;
  }

  if (!match) {
    debugPrintf("GameUI patch: save dialog instruction pattern unavailable\n");
    return 0;
  }

  *match = skip_fastswitch;
  debugPrintf("GameUI patch: preserving hud_fastswitch across save/load\n");
  return 1;
}

void apply_game_patches(void) {
  so_module *gameui = so_find_module("libGameUI.so");
  if (gameui)
    patch_save_dialog_fastswitch(gameui);
}
