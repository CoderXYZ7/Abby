#!/usr/bin/env python3
import os
import json
import sys
import glob

def generate_catalog(audio_dir, output_file):
    tracks = []
    
    # Supported extensions
    extensions = ['*.pira'] # Encrypted files
    
    files = []
    for ext in extensions:
        files.extend(glob.glob(os.path.join(audio_dir, ext)))
        
    files.sort()
    
    print(f"Found {len(files)} encrypted tracks in {audio_dir}")
    
    for i, filepath in enumerate(files):
        filename = os.path.basename(filepath)
        # remove extension for title
        title = os.path.splitext(filename)[0]
        # remove track prefix if present (e.g. track1 -> 1)
        if title.startswith("track"):
            title = title[5:]
            
        track_id = str(i + 1)
        
        tracks.append({
            "id": track_id,
            "title": title,
            "path": filepath,
            "requiredPermission": "1" # Default permission
        })
        
    catalog = {"tracks": tracks}
    
    with open(output_file, 'w') as f:
        json.dump(catalog, f, indent=2)
        
    print(f"Generated catalog with {len(tracks)} tracks at {output_file}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 generate_catalog.py <audio_dir> <output_json>")
        sys.exit(1)
        
    generate_catalog(sys.argv[1], sys.argv[2])
