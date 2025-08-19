# 1.2.1

## Bug fixes
- Potential fix for auto support proxies getting lost on loading a save. The trace sometimes does not return the pieces
 that should be there and I suspect it's due to lightweight system not being fully initialized when the trace executes at
 BeginPlay. This only seems to happen sometimes. Delaying the trace by a single tick seems to fix the issue.
- Fix an issue where the auto support dismantle mode stops working after respawning due to death.

# 1.2.0

## New features
- Add basic snap support underneath catwalks (in-place rotation and grid snap not yet implemented)
- Add basic snap support on single beam connectors (in-place rotation not yet implemented)
- Add a tooltip to the build button in the interact dialog to advertise the auto-build keybind.

## Bug fixes
- Fix cases where a support cannot be built in smaller spaces with certain configurations.
- Add disqualifiers for some cases when a build cannot be performed.

## Minor changes
- Swap the location of the save and load button next to the preset dropdown.
- Adjust logging. TBD: Reduce log spam

# 1.1.0

## Changes
- Fix landscape meshes (cliffs, caves, rocks) not being considered landscape hits. When the "Only Intersect Terrain"
  mode is enabled, these will no longer be ignored. Foliage will always be ignored.
- Improved the preset UX. By default, "\[New\]" will be selected. To load a preset, either choose it from the dropdown
  or press the load button next to the combo to reload the selected preset. The selected preset is no longer synced 
  between cubes to avoid confusion.
- Fix build disqualifiers not showing up in certain cases.
- Added mod configuration setting for manual exclusion of part categories from the Auto Support part picker.
- Removed obsolete mod configuration settings that weren't being used.
- Auto Support cubes are now excluded from the part picker to avoid unpredictable behaviors.

## Under the hood
- Added content registry tags to tag assets. These tags affect the behaviors of the part picker and support building.
- Classify meshes under certain subfolders of the vanilla content folder "World/Environment" as landscape or ignored during
  traces.

# 1.0.0

- Initial release.