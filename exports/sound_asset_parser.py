#!/usr/bin/env python3
"""
TAZ: WANTED - Sound Asset Parser
Extracts and categorizes audio files from .pc archives
"""

import os
import json
import struct
from pathlib import Path

class SoundAssetParser:
    """Parse sound assets from Taz: Wanted game files"""
    
    LANGUAGE_CODES = {
        'd': 'default',
        'e': 'english',
        'f': 'french', 
        'it': 'italian',
        'uk': 'uk_english'
    }
    
    def __init__(self, pak_dir):
        self.pak_dir = Path(pak_dir)
        
    def find_sound_files(self):
        """Find all sound files in PAK directory"""
        sound_files = []
        
        for file in sorted(self.pak_dir.glob('*.pc')):
            if 'sound' not in file.stem.lower():
                continue
                
            try:
                size = file.stat().st_size
                magic = file.read(8).hex()[:16]
                
                sound_files.append({
                    'path': str(file),
                    'filename': file.name,
                    'size_bytes': size,
                    'magic_bytes': magic,
                    'entity_base': '_'.join(name for name in file.stem.split('_') if name.isdigit() or name.isalpha()),
                    'language_code': None  # Would be parsed from filename suffix
                })
            except Exception as e:
                pass
                
        return sound_files
    
    def categorize_by_entity(self):
        """Group sounds by their source entity"""
        entities = {}
        
        for file in self.find_sound_files():
            # Extract entity name from filename (e.g., "tazsound_d.pc" -> taz)
            parts = file['filename'].replace('.pc', '').rsplit('_', 2)
            if len(parts) >= 2:
                entity_name = parts[0] + parts[1]
                if entity_name not in entities:
                    entities[entity_name] = []
                entities[entity_name].append(file)
                
        return {k: sorted(v, key=lambda x: str(x['size_bytes'])) for k, v in entities.items()}
    
    def parse_audio_header(self, filepath):
        """Parse audio file header structure"""
        try:
            with open(filepath, 'rb') as f:
                header = f.read(64)
                
            # Simplified header parsing
            if len(header) >= 8:
                return {
                    'magic': header[:8].hex(),
                    'format_marker': (header[8] == 0x10),
                    'has_wave_data': (len(header) > 64)
                }
        except:
            pass
        
        return None
    
    def generate_report(self):
        """Generate comprehensive sound asset report"""
        sounds = self.categorize_by_entity()
        
        report = {
            'project': 'Taz: Wanted Audio System',
            'total_sound_files': sum(len(v) for v in sounds.values()),
            'unique_entities': len(sounds),
            'language_variants': list(self.LANGUAGE_CODES.keys()),
            'entity_sounds': {k: [s['size_bytes'] for s in v] for k, v in sorted(sounds.items(), key=lambda x: -sum([i['size_bytes'] for i in y]))[:10]},
            'analysis_date': '2026-10-19'
        }
        
        return report
    
    def save_report(self, output_path):
        """Save sound asset report to JSON"""
        report = self.generate_report()
        
        with open(output_path, 'w') as f:
            json.dump(report, f, indent=2)
            
        return report


def main():
    parser = SoundAssetParser('/home/mochi/taz-decomp/bin/Paks/')
    
    print("=== TAZ: WANTED Sound Asset Analysis ===\n")
    
    report = parser.save_report(
        '/home/mochi/taz-decomp/exports/sound_assets_analysis.json'
    )
    
    print(f"Total sound files found: {report['total_sound_files']}")
    print(f"Unique entities with sounds: {report['unique_entities']}")
    print(f"\nSample entity-sound relationships:")
    for entity, sizes in list(report['entity_sounds'].items())[:5]:
        print(f"  {entity}: {len(sizes)} variants, avg size ~{sum(sizes)/len(sizes):.1f}B")
    
    return report

if __name__ == '__main__':
    main()
