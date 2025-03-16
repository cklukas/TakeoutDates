# Google Photos Takeout Date Setter

This is a small tool to process Google Photos Takeout exports, setting file creation and modification times based on metadata.

It is not an official Google product and developed by an independent developer. The tool is provided as-is, without any warranty or support. For me, it has worked well with my own Google Photos Takeout exports, but your mileage may vary. 

Check the output (--list option) before setting file dates to ensure the metadata is correctly parsed.

Do not use this tool if you are not comfortable with the risks of modifying file timestamps. Always back up your files before running this tool.

## Features

- Recursively scans a folder for '.supplemental-metadata.json' files.
- Extracts 'photoTakenTime' (creation time) and 'creationTime' (upload time) from JSON metadata.
- Updates the primary file (e.g., 'IMG_7014.HEIC') and an associated '.MP4' file (e.g., 'IMG_7014.MP4') if it exists and lacks its own metadata.
- Supports listing files with timestamps or setting file dates.
- Cross-platform, with creation time support on macOS (APFS/HFS+).

## Prerequisites

- C++17 compatible compiler.
- CMake 3.10 or higher.
- 'nlohmann/json' library (fetched automatically via CMake).

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
- '--list': Output CSV of filenames, photo taken time, and upload time to stdout.
- '--set-file-dates': Set file creation (photo taken) and modification (upload) times.

### Example

List all files and their timestamps:
```
google_photos_date_setter /path/to/photos --list
```

Set file dates:
```
google_photos_date_setter /path/to/photos --set-file-dates
```

## CSV Output Format

When using '--list', outputs (only primary file listed):
```
File,PhotoTakenTime,UploadTime
"/path/to/IMG_7014.HEIC","2018-10-04 14:32:12","2021-10-17 10:49:08"
```

## Notes

- Timestamps are in UTC, formatted as 'YYYY-MM-DD HH:MM:SS'.
- Requires metadata files in the format produced by Google Photos Takeout.
- Updates both the primary file (e.g., '.HEIC', '.JPG') and an associated '.MP4' file if present, using the primary file's metadata.
- Creation time updates require macOS with APFS or HFS+ file systems (not supported on exFAT/FAT32).

## License

MIT License
