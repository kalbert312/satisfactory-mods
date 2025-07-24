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