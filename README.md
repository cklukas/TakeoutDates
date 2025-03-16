# Google Photos Takeout Date Setter

This is a small tool to process Google Photos Takeout exports, setting file creation and modification times based on metadata, and optionally managing Finder Tags on macOS.

It is not an official Google product and developed by an independent developer. The tool is provided as-is, without any warranty or support. For me, it has worked well with my own Google Photos Takeout exports, but your mileage may vary.

Check the output (--list option) before setting file dates or tags to ensure the metadata is correctly parsed.

Do not use this tool if you are not comfortable with the risks of modifying file timestamps or tags. Always back up your files before running this tool.

During execution (--set-file-dates or tag-related options), the tool will update files but will not output progress information unless errors occur.

## Features

- Recursively scans a folder for '.supplemental-metadata.json' and '.suppl.json' files.
- Extracts 'photoTakenTime' (creation time) and 'creationTime' (upload time) from JSON metadata.
- Updates the primary file (e.g., 'IMG_7014.HEIC') and an associated '.MP4' file (e.g., 'IMG_7014.MP4') if it exists and lacks its own metadata.
- Supports listing files with timestamps and people names, setting file dates, and managing Finder Tags (macOS only).
- Cross-platform, with creation time and tag support on macOS (APFS/HFS+).

## Prerequisites

- C++17 compatible compiler.
- CMake 3.10 or higher.
- 'nlohmann/json' library (fetched automatically via CMake).
- macOS with Xcode (for Finder Tag support, requires Foundation framework).

## Building
```
mkdir build
cd build
cmake ..
make
```

## Usage
```
google_photos_date_setter <folder> [options]
```

### Options

- '--help': Display help message.
- '--list': Output CSV of filenames, photo taken time, upload time, and people names (semicolon-separated).
- '--set-file-dates': Set file creation (photo taken) and modification (upload) times.
- '--assign-people-tags "tag1;..."': Assign specified Finder Tags from JSON 'people' names (macOS only, semicolon-separated).
- '--assign-all-people-tags': Assign all 'people' names from JSON as Finder Tags (macOS only).
- '--list-tags': List unique 'people' names from JSON files.
- '--remove-all-tags': Remove all Finder Tags from files (macOS only).
- '--remove-named-tags "tag1;..."': Remove specific Finder Tags (macOS only, semicolon-separated).

### Example

List all files with timestamps and people:
```
google_photos_date_setter /path/to/photos --list
```

Set file dates:
```
google_photos_date_setter /path/to/photos --set-file-dates
```

List unique people tags:
```
google_photos_date_setter /path/to/photos --list-tags
```

Assign specific people tags:
```
google_photos_date_setter /path/to/photos --assign-people-tags "Sarah;Christian"
```

Assign all people tags from JSON:
```
google_photos_date_setter /path/to/photos --assign-all-people-tags
```

Remove all tags:
```
google_photos_date_setter /path/to/photos --remove-all-tags
```

Remove specific tags:
```
google_photos_date_setter /path/to/photos --remove-named-tags "Sarah;Christian"
```

## CSV Output Format

When using '--list', outputs primary file and associated .MP4 (if applicable) with people names:
```
File,PhotoTakenTime,UploadTime,People
"/path/to/IMG_7014.HEIC","2018-10-04 14:32:12","2021-10-17 10:49:08","Christian;Sarah"
"/path/to/IMG_7014.MP4","2018-10-04 14:32:12","2021-10-17 10:49:08","Christian"
```

## Notes

- Timestamps are in UTC, formatted as 'YYYY-MM-DD HH:MM:SS'.
- Requires metadata files in the format produced by Google Photos Takeout ('.supplemental-metadata.json' or '.suppl.json').
- Updates both the primary file (e.g., '.HEIC', '.JPG') and an associated '.MP4' file if present, using the primary file's metadata.
- Finder Tag features (--assign-people-tags, --assign-all-people-tags, --remove-all-tags, --remove-named-tags) are macOS-only and require APFS or HFS+ file systems (not supported on exFAT/FAT32).
- People names in '--list' and '--assign-people-tags' use semicolon (;) as separator for consistency.

## License

MIT License