#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
TAZ: WANTED - Batch Summary Generator
Processes all parsed .pc files and generates statistics report

Author: Mochibunn Auto-Toolchain v4.0
Project: /home/mochi/taz-wanted-native
"""

import json
import sys
from pathlib import Path


def analyze_file_magic(file_path, magic_bytes):
    """Analyze detected file magic byte patterns."""
    
    analysis = {
        'file': str(file_path),
        'size': None,  # Will be filled by parser
        'magic': magic_bytes.hex(),
        'type_guess': 'unknown'
    }
    
    # Known magic bytes from our parsing
    if magic_bytes.hex() == '4704bb19':
        analysis['type_guess'] = 'Character Model (taz)'
    elif magic_bytes.hex().startswith('5b'):
        analysis['type_guess'] = 'NPC Model'
    elif magic_bytes.hex().startswith('e3') or len(magic_bytes) == 4:
        analysis['type_guess'] = 'Sound/Audio Asset'
    elif magic_bytes.hex().startswith('ad') or magic_bytes.hex().endswith('d2'):
        analysis['type_guess'] = 'Localized Audio (DE/UK)'
    elif magic_bytes.hex().endswith('7b'):
        analysis['type_guess'] = 'Localized Audio (ES/French variants)'
    
    return analysis


def generate_summary(all_parsed_files):
    """Generate comprehensive batch summary report."""
    
    summary = {
        'project': 'TAZ: WANTED Native Port',
        'date': '2026-10-19',
        'total_files_scanned': len(all_parsed_files),
        'file_types': {},
        'size_statistics': {
            'min': None,
            'max': None,
            'avg': None,
            'median': None
        },
        'magic_bytes_seen': set(),
        'files_by_magic': {},
        'ready_for_extraction': []
    }
    
    # Analyze each file
    total_size = 0
    
    for filename, data in all_parsed_files.items():
        size = data.get('file_size', 0) or 0
        
        if summary['size_statistics']['min'] is None or size < summary['size_statistics']['min']:
            summary['size_statistics']['min'] = size
        
        if summary['size_statistics']['max'] is None or size > summary['size_statistics']['max']:
            summary['size_statistics']['max'] = size
        
        total_size += size
    
    # Add magic byte analysis
    for filename, data in all_parsed_files.items():
        magic_hex = data.get('structure_analysis', {}).get('format_magic', '').upper()
        
        if magic_hex not in summary['files_by_magic']:
            summary['files_by_magic'][magic_hex] = []
            
        summary['files_by_magic'][magic_hex].append({
            'file': filename,
            'size': data.get('file_size'),
            'type': data.get('structure_analysis', {}).get('container_type', {})
        })
        
        if magic_hex not in summary['magic_bytes_seen']:
            summary['magic_bytes_seen'].add(magic_hex)
    
    # Compute statistics
    sizes = [d.get('file_size') or 0 for d in all_parsed_files.values()]
    valid_sizes = [s for s in sizes if s > 0]
    
    if valid_sizes:
        summary['size_statistics']['min'] = min(valid_sizes)
        summary['size_statistics']['max'] = max(valid_sizes)
        summary['size_statistics']['avg'] = sum(valid_sizes) / len(valid_sizes)
        
        # Simple median calculation
        sorted_sizes = sorted(valid_sizes)
        mid = len(sorted_sizes) // 2
        if len(sorted_sizes) % 2 == 0:
            summary['size_statistics']['median'] = (sorted_sizes[mid-1] + sorted_sizes[mid]) / 2
        else:
            summary['size_statistics']['median'] = sorted_sizes[mid]
            
    # Calculate total data volume
    summary['total_data_volume_bytes'] = total_size
    summary['total_data_volume_mb'] = total_size / (1024 * 1024)
    
    return summary


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='TAZ: WANTED Batch Summary Generator')
    parser.add_argument('--input-dir', '-i', 
                        default='/exports/taz-wanted-assets',
                        help='Directory containing .json extraction reports')
    parser.add_argument('--output', '-o',
                        default='/home/mochi/taz-decomp/exports/summary.json',
                        help='Output path for summary report')
    
    args = parser.parse_args()
    
    input_dir = Path(args.input_dir)
    
    if not input_dir.exists():
        print(f"Error: Input directory not found: {input_dir}")
        print("Run batch parser first: python3 advanced_pc_parser.py --batch=...")
        return 1
        
    # Find all JSON extraction reports
    json_files = list(input_dir.glob('*.json'))
    
    if len(json_files) == 0:
        print(f"No .json files found in {input_dir}")
        return 0
        
    # Parse each report
    all_parsed = {}
    for json_file in sorted(json_files):
        try:
            with open(json_file) as f:
                data = json.load(f)
                
            # Extract filename from path
            filename = json_file.stem
            
            # Add to collection (skip duplicates, last one wins)
            all_parsed[filename] = data
                
        except Exception as e:
            print(f"Warning: Could not parse {json_file}: {e}")
    
    # Generate summary
    summary = generate_summary(all_parsed)
    
    # Write summary
    with open(args.output, 'w') as f:
        json.dump(summary, f, indent=2, sort_keys=True)
    
    print(f"\n=== TAZ: WANTED Batch Summary ===")
    print(f"Files scanned: {len(json_files)}")
    print(f"Total data volume: {summary['total_data_volume_mb']:.1f} MB")
    print(f"\nMagic bytes observed ({len(summary['magic_bytes_seen'])} unique):")
    
    for magic_hex in summary['magic_bytes_seen']:
        files = summary['files_by_magic'].get(magic_hex, [])
        print(f"  {magic_hex}: {len(files)} files")
        
    print(f"\nSummary report written to: {args.output}")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
